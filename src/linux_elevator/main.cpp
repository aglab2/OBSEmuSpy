#include "../elevator_proto.h"

#include <stdlib.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/uio.h>

#include <stdio.h>

static inline ssize_t ReadProcessMemory(pid_t pid, void *remotePtr,
					void *localPtr, size_t sz)
{
	struct iovec lvec[] = {{
		.iov_base = localPtr,
		.iov_len = sz,
	}};
	struct iovec rvec[] = {{
		.iov_base = remotePtr,
		.iov_len = sz,
	}};

	return process_vm_readv(pid, lvec, 1, rvec, 1, 0);
}

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

int main(int argc, char *argv[])
{
	if (argc != 2) {
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	int fd = atoi(argv[1]);
	ElevatorProto::InMessage inMsg;

	ssize_t readSz;

	while ((readSz = robustRead(fd, &inMsg, sizeof(inMsg))) ==
	       sizeof(inMsg)) {
		size_t size = inMsg.size + sizeof(ElevatorProto::OutMessage);
		ElevatorProto::OutMessage *outMsg =
			(ElevatorProto::OutMessage *)malloc(size);

		outMsg->size = (int32_t)ReadProcessMemory(inMsg.pid,
							  (void *)inMsg.address,
							  outMsg->data,
							  inMsg.size);
		outMsg->error = errno;

		ssize_t sz = robustWrite(fd, outMsg, size);
		if (sz != (ssize_t)size) {
			printf("Elevator: robustWrite failed with %zd, errno=%d\n",
			       sz, errno);
			return 0;
		}

		free(outMsg);
	}

	printf("Elevator: readSz=%zd, errno=%d\n", readSz, errno);

	return 0;
}
