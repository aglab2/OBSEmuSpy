#include "emulator.h"

#include "plugin-support.h"

#ifdef _WIN32
#include <psapi.h>
#else
#include <dirent.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "input.h"
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#define process_ pid_
#endif

Emulator::Emulator() : thread_(&Emulator::work, this) {}

Emulator::~Emulator()
{
	{
		std::lock_guard<std::mutex> lck(mutex_);
		running_ = false;
	}
	cv_.notify_one();
	thread_.join();
}

#ifndef _WIN32
#define WAIT_OBJECT_0 0
#define WaitForSingleObject(...) 1
static bool ReadProcessMemory(pid_t pid, void *remotePtr, void *localPtr,
			      size_t sz, void *_)
{
	(void)_;
	struct iovec lvec[] = {{
		.iov_base = localPtr,
		.iov_len = sz,
	}};
	struct iovec rvec[] = {{
		.iov_base = remotePtr,
		.iov_len = sz,
	}};

	ssize_t nread = process_vm_readv(pid, lvec, 1, rvec, 1, 0);
	printf("ReadProcessMemory: read %zd bytes from %p: %d\n", nread, remotePtr, errno);
	return (ssize_t)sz == nread;
}
#endif

#if _WIN32
static std::string moduleNameLowerCase(HANDLE process, HMODULE module)
{
	std::string name;
	name.resize(MAX_PATH);
	int len = GetModuleBaseNameA(process, module, name.data(),
				     (DWORD)name.size());
	if (0 == len)
		return {};

	name.resize(len);
	std::transform(name.begin(), name.end(), name.begin(),
		       [](unsigned char c) { return std::tolower(c); });

	return name;
}

void Emulator::searchProcess()
{
	msToWait_ = 1000;
	DWORD pids[1024], needed;
	if (!EnumProcesses(pids, sizeof(pids), &needed))
		return;

	DWORD count = needed / sizeof(DWORD);
	for (DWORD i = 0; i < count; i++) {
		DWORD pid = pids[i];
		if (0 == pid)
			continue;

		WinHandle process{
			OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION |
					    PROCESS_VM_READ,
				    FALSE, pid)};
		if (!process)
			continue;

		HMODULE mainModule;
		DWORD needed;
		if (!EnumProcessModules(process, &mainModule, sizeof(HMODULE),
					&needed))
			continue;

		std::string name = moduleNameLowerCase(process, mainModule);
		bool pj64 = name == "project64.exe";
		bool retroarch = name == "retroarch.exe";
		if (!pj64 && !retroarch)
			continue;

		type_ = pj64 ? EmulatorType::PJ64 : EmulatorType::RETROARCH;
		pid_ = pid;
		process_ = std::move(process);
		IsWow64Process(process_, &processIs64Bit_);
		break;
	}
}

void Emulator::scanProcessRAM()
{
	msToWait_ = 1000;
	if (WAIT_OBJECT_0 == WaitForSingleObject(process_, 0)) {
		markProcessDead();
		return;
	}

	PVOID MaxAddress = processIs64Bit_ ? (PVOID)0x800000000000ULL
					   : (PVOID)0xffffffffULL;
	PVOID address = nullptr;
	const int offset = 0; // TODO: Change for mupen
	uint8_t *ramPtrBase = nullptr;

	if (type_ == EmulatorType::PJ64) {
		do {
			MEMORY_BASIC_INFORMATION m;
			SIZE_T mbiSize = sizeof(m);
			SIZE_T result =
				VirtualQueryEx(process_, address, &m, mbiSize);
			if (address == (char *)m.BaseAddress + m.RegionSize ||
			    result == 0)
				break;

			DWORD prot = m.Protect & 0xff;
			if (prot == PAGE_EXECUTE_READWRITE ||
			    prot == PAGE_EXECUTE_WRITECOPY ||
			    prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
			    prot == PAGE_READONLY) {
				uint8_t *ramPtrBaseCandidate =
					(uint8_t *)m.BaseAddress + offset;
				if (probeRAMAddress(ramPtrBaseCandidate)) {
					ramPtrBase = ramPtrBaseCandidate;
					break;
				}
			}

			address = (uint8_t *)m.BaseAddress + m.RegionSize;
		} while (address <= MaxAddress);
	} else {
		HMODULE modules[1024];
		DWORD bytesNeeded;

		if (!EnumProcessModules(process_, modules, sizeof(modules),
					&bytesNeeded))
			return;

		int moduleCount = bytesNeeded / sizeof(HMODULE);
		for (int i = 0; i < moduleCount; ++i) {
			HMODULE module = modules[i];
			std::string name =
				moduleNameLowerCase(process_, module);
			if (name.find("parallel_n64") == std::string::npos)
				continue;

			MODULEINFO mi;
			if (0 == GetModuleInformation(process_, module, &mi,
						      sizeof(mi)))
				continue;

			uint8_t *candidateRamPtrBase =
				(uint8_t *)mi.lpBaseOfDll;
			uint8_t *parallelEnd =
				candidateRamPtrBase + mi.SizeOfImage;
			while (candidateRamPtrBase < parallelEnd) {
				if (probeRAMAddress(candidateRamPtrBase)) {
					ramPtrBase = candidateRamPtrBase;
					break;
				}
				candidateRamPtrBase += 0x1000;
			}

			if (ramPtrBase)
				break;
		}
	}

	if (!ramPtrBase)
		return;

	std::vector<uint32_t> ram;
	ram.resize(0x100000);
	if (!ReadProcessMemory(process_, ramPtrBase, ram.data(), 0x400000,
			       nullptr))
		return;

	analyzeResult_ = MIPS::analyze(ram);
	if (!analyzeResult_)
		return;

	ramPtrBase_ = ramPtrBase;
}
#define translateInputs(x) x
#else
bool Emulator::probeRAMAddress(void *ramPtrBaseCandidate)
{
	const uint32_t ramMagic = 0x3C1A8000;
	const uint32_t ramMagicMask = 0xfffff000;

	printf("Probing RAM address %p\n", ramPtrBaseCandidate);

	uint32_t value;
	if (!ReadProcessMemory(process_, ramPtrBaseCandidate, &value,
			       sizeof(value), nullptr))
		return false;
	
	printf("Probing RAM address %p, read value 0x%08x\n", ramPtrBaseCandidate, value);

	if ((value & ramMagicMask) == ramMagic) {
		printf("Found RAM base at %p\n", ramPtrBaseCandidate);
		return true;
	}

	printf("Address %p is not RAM\n", ramPtrBaseCandidate);
	return false;
}

void Emulator::searchProcess()
{
	msToWait_ = 1000;
	DIR *dir = opendir("/proc");
	while (auto entry = readdir(dir)) {
		const char *name = entry->d_name;
		if (name[0] == '.')
			continue;

		pid_t pid = atoi(name);
		if (!pid)
			continue;

		char commPath[100];
		sprintf(commPath, "/proc/%d/comm", pid);
		char comm[32];

		int fd = open(commPath, O_RDONLY);
		if (fd < 0)
			continue;

		ssize_t sz = read(fd, comm, sizeof(comm));
		close(fd);

		if (sz <= 1)
			continue;

		std::string_view commSV{comm, (size_t)sz - 1};

		bool isDolphin = commSV == "dolphin-emu";
		bool isPJ64 = commSV == "Project64.exe";
		if (!isDolphin && !isPJ64)
			continue;

		type_ = isDolphin ? EmulatorType::DOLPHIN : EmulatorType::PJ64;
		pid_ = pid;
		processIs64Bit_ = true;
		break;
	}
	closedir(dir);
}

void Emulator::scanDolphinRAM(std::ifstream &file)
{
	std::string line;
	std::string needle = "/bin/dolphin-emu";
	while (std::getline(file, line)) {
		// 55a832538000-55a83256b000 rw-p 01820000 103:04 7087472                   /app/bin/dolphin-emu
		if (line.find(needle) == std::string::npos)
			continue;

		auto dash = line.find('-');
		if (dash == std::string::npos)
			continue;
		auto space = line.find(' ');
		if (space == std::string::npos)
			continue;

		std::string_view lineSV{line};
		std::string_view perms = lineSV.substr(space + 1, 4); // rw-p
		if (perms.length() != 4)
			continue;

		if ('w' != perms[1])
			continue;

		std::string startStr = line.substr(0, dash); // 55a832538000
		uint64_t start = strtoul(startStr.c_str(), nullptr, 16);

		ramPtrBase_ = (uint8_t *)start;
		analyzeResult_ = MIPS::AnalyzeResult{
			.interpretedInstructionsOffset = 0x16ed8 / 4,
			.interpretedInstructions = {0x76726553,
						    0x00007265}, // Server\0\0
			.gControllerPads = 0x16e0c,
		};

		break;
	}
}

void Emulator::scanProject64RAM(std::ifstream &file)
{
	std::string line;
	uint8_t *ramPtrBase = nullptr;
	while (std::getline(file, line)) {
		// 02480000-02660000 rwxs 00000000 00:01 6365                               /memfd:wine-mapping (deleted)
		auto dash = line.find('-');
		if (dash == std::string::npos)
			continue;
		auto space = line.find(' ');
		if (space == std::string::npos)
			continue;

		std::string_view lineSV{line};
		std::string_view perms = lineSV.substr(space + 1, 4); // rw-p
		if (perms.length() != 4)
			continue;

		if ('w' != perms[1] && 'r' != perms[0])
			continue;

		std::string startStr = line.substr(0, dash); // 55a832538000
		uint64_t start = strtoul(startStr.c_str(), nullptr, 16);
		uint64_t end =
			strtoul(line.substr(dash + 1, space - dash - 1).c_str(),
				nullptr, 16);

		if (end - start < 0x400000)
			continue;

		uint8_t *ramPtrBaseCandidate = (uint8_t *)start;
		if (probeRAMAddress(ramPtrBaseCandidate)) {
			ramPtrBase = ramPtrBaseCandidate;
			break;
		}
	}

	if (!ramPtrBase)
		return;

	std::vector<uint32_t> ram;
	ram.resize(0x100000);
	if (!ReadProcessMemory(process_, ramPtrBase, ram.data(), 0x400000,
			       nullptr))
		return;

	analyzeResult_ = MIPS::analyze(ram);
	if (!analyzeResult_)
		return;

	ramPtrBase_ = ramPtrBase;
}

void Emulator::scanProcessRAM()
{
	msToWait_ = 1000;
	char mapPath[100];
	sprintf(mapPath, "/proc/%d/maps", process_);

	std::ifstream file(mapPath);
	if (!file.is_open()) {
		markProcessDead();
		return;
	}

	if (type_ == EmulatorType::DOLPHIN) {
		scanDolphinRAM(file);
	} else if (type_ == EmulatorType::PJ64) {
		scanProject64RAM(file);
	} else {
		markProcessDead();
	}
}

enum N64Name {
	N64_CRight,
	N64_CLeft,
	N64_CDown,
	N64_CUp,
	N64_R,
	N64_L,
	N64_X,
	N64_Y,

	N64_Right,
	N64_Left,
	N64_Down,
	N64_Up,
	N64_Start,
	N64_Z,
	N64_B,
	N64_A,
};

enum DolphinName {
	GC_A,     // N64_CRight
	GC_B,     // N64_CLeft
	GC_X,     // N64_CDown
	GC_Y,     // N64_CUp
	GC_Start, // N64_R
	GC_CLeft, // N64_L
	GC_CDown, // N64_X
	GC_CUp,   // N64_Y

	GC_Left,   // N64_Right
	GC_Right,  // N64_Left
	GC_Down,   // N64_Down
	GC_Up,     // N64_Up
	GC_Z,      // N64_Start
	GC_R,      // N64_Z
	GC_L,      // N64_B
	GC_CRight, // N64_A
};

static uint32_t translateInput(uint32_t value)
{
	value = __builtin_bswap32(value);
	struct Input input;
	__builtin_memcpy(&input, &value, sizeof(input));

	input.x -= 0x80;
	input.y -= 0x80;

	uint16_t dolphinFlags = 0;
#define TRANSLATE(flag)                            \
	if (input.flags & (1 << GC_##flag)) {      \
		dolphinFlags |= (1 << N64_##flag); \
	}
	TRANSLATE(CRight)
	TRANSLATE(CLeft)
	TRANSLATE(CDown)
	TRANSLATE(CUp)
	TRANSLATE(R)
	TRANSLATE(L)
	TRANSLATE(X)
	TRANSLATE(Y)

	TRANSLATE(Right)
	TRANSLATE(Left)
	TRANSLATE(Down)
	TRANSLATE(Up)
	TRANSLATE(Start)
	TRANSLATE(Z)
	TRANSLATE(B)
	TRANSLATE(A)
#undef TRANSLATE
	input.flags = dolphinFlags;

	__builtin_memcpy(&value, &input, sizeof(input));

	return value;
}
#endif

int32_t Emulator::feedInputs()
{
	msToWait_ = 15;
	if (WAIT_OBJECT_0 == WaitForSingleObject(process_, 0)) {
		markProcessDead();
		return 0;
	}

	std::vector<uint32_t> verifier;
	verifier.reserve(analyzeResult_->interpretedInstructions.size());
	if (!ReadProcessMemory(
		    process_,
		    ramPtrBase_ +
			    analyzeResult_->interpretedInstructionsOffset *
				    sizeof(uint32_t),
		    verifier.data(),
		    analyzeResult_->interpretedInstructions.size() *
			    sizeof(uint32_t),
		    nullptr)) {
		markRAMDead();
		return 0;
	}

	if (0 != memcmp(verifier.data(),
			analyzeResult_->interpretedInstructions.data(),
			analyzeResult_->interpretedInstructions.size() *
				sizeof(uint32_t))) {
		markRAMDead();
		return 0;
	}

	uint32_t inputs;
	if (!ReadProcessMemory(process_,
			       ramPtrBase_ + (analyzeResult_->gControllerPads &
					      0xffffff),
			       &inputs, sizeof(inputs), nullptr)) {
		return 0;
	}

	if (EmulatorType::DOLPHIN == type_) {
		return translateInput(inputs);
	}
	return inputs;
}

void Emulator::work()
{
	std::unique_lock<std::mutex> lck(mutex_);
	while (running_) {
		cv_.wait_for(lck, std::chrono::milliseconds(msToWait_));
		lck.unlock();

		int32_t inputs = 0;
		if (!process_) {
			searchProcess();
		}

		if (process_ && !ramPtrBase_) {
			scanProcessRAM();
		}

		if (analyzeResult_) {
			inputs = feedInputs();
		}
		lck.lock();
		inputs_.store(inputs, std::memory_order_relaxed);
	}
}

void Emulator::markProcessDead()
{
	process_ = {};
	markRAMDead();
}

void Emulator::markRAMDead()
{
	ramPtrBase_ = nullptr;
	analyzeResult_.reset();
}
