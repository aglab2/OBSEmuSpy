#include "elevator.h"

#include "elevator_proto.h"

#include <string>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>

Elevator *gElevator = nullptr;

static inline ssize_t robustRead(int fd, void *buf, size_t count)
{
retry:
	ssize_t sz = read(fd, buf, count);
	if (sz < 0) {
		if (errno == EINTR)
			goto retry;

		return -1;
	}

	return sz;
}

static inline ssize_t robustWrite(int fd, const void *buf, size_t count)
{
retry:
	ssize_t sz = write(fd, buf, count);
	if (sz < 0) {
		if (errno == EINTR)
			goto retry;

		return -1;
	}

	return sz;
}

Elevator::Elevator()
{
	int fds[2];
	socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds);

	socklen_t optlen = sizeof(maxPacketSize_);
	int r = getsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &maxPacketSize_,
			   &optlen);
	if (r < 0) {
		maxPacketSize_ = 65536;
	} else {
		// Leave some headroom for protocol overhead. iirc real limits are 32 bytes but I am being safe
		maxPacketSize_ -= 128;
	}

	fd_ = fds[0];
	pid_ = fork();
	if (pid_ == 0) {
		char fdStr[20];
		snprintf(fdStr, sizeof(fdStr), "%d", fds[1]);

		close(fds[0]);
		execl("/usr/local/bin/obs_elevator", "linux_elevator", fdStr,
		      nullptr);
		_exit(1);
	} else {
		close(fds[1]);
	}
}

Elevator::~Elevator()
{
	close(fd_);
	waitpid(pid_, nullptr, 0);
}

int Elevator::requestRead(pid_t pid, void *remotePtr, void *localPtr, int chunk)
{
	ElevatorProto::InMessage inMsg{
		.pid = pid,
		.size = chunk,
		.address = (uint64_t)remotePtr,
	};

	{
		if (robustWrite(fd_, &inMsg, sizeof(inMsg)) != sizeof(inMsg)) {
			return -1;
		}
	}

	size_t outMsgSize = sizeof(ElevatorProto::OutMessage) + chunk;
	auto outMsg = (ElevatorProto::OutMessage *)malloc(outMsgSize);

	{
		if (robustRead(fd_, outMsg, outMsgSize) !=
		    (ssize_t)outMsgSize) {
			free(outMsg);
			return -1;
		}
	}

	errno = outMsg->error;
	memcpy(localPtr, outMsg->data, chunk);
	int result = outMsg->size;

	free(outMsg);

	return result;
}

ssize_t Elevator::processRead(pid_t pid, void *remotePtr, void *localPtr,
			      size_t sz)
{
	ssize_t total = 0;
	while (sz > 0) {
		int chunk = sz < (size_t)maxPacketSize_ ? (int)sz
							: maxPacketSize_;
		int result = requestRead(pid, remotePtr, localPtr, chunk);
		if (result < 0) {
			return -1;
		}
		if (result == 0) {
			break;
		}

		total += result;
		sz -= result;
		remotePtr = (void *)((uintptr_t)remotePtr + result);
		localPtr = (void *)((uintptr_t)localPtr + result);
	}

	return total;
}
