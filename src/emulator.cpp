#include "emulator.h"

#include <psapi.h>

#include <algorithm>
#include <cctype>
#include <vector>

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
		bool mupen = name == "mupen64.exe";
		if (!pj64 && !retroarch && !mupen)
			continue;

		type_ = EmulatorType::UNKNOWN;
		if (pj64)
			type_ = EmulatorType::PJ64;
		else if (retroarch)
			type_ = EmulatorType::RETROARCH;
		else if (mupen)
			type_ = EmulatorType::MUPEN;

		pid_ = pid;
		process_ = std::move(process);
		IsWow64Process(process_, &processIs64Bit_);
		break;
	}
}

const uint32_t ramMagic = 0x3C1A8000;
const uint32_t ramMagicMask = 0xfffff000;

bool Emulator::probeRAMAddress(void *ramPtrBaseCandidate)
{
	uint32_t value;
	if (!ReadProcessMemory(process_, ramPtrBaseCandidate, &value,
			       sizeof(value), nullptr))
		return false;

	if ((value & ramMagicMask) == ramMagic) {
		return true;
	}

	return false;
}

uint64_t Emulator::scanForRAM(void *address, uint64_t size, uint64_t delim)
{
	bool isRamFound = false;
	uint64_t ramPtrBase = 0;
	uint64_t addressAlignedStart =
		((uint64_t)address + delim - 1) / delim * delim;
	uint64_t addressAlignedEnd = ((uint64_t)address + size) / delim * delim;

	for (uint64_t probe = addressAlignedStart; probe <= addressAlignedEnd;
	     probe += delim) {
		uint32_t value;
		bool readSuccess = ReadProcessMemory(process_, (void *)probe,
						     &value, sizeof(value),
						     nullptr);
		if (readSuccess) {
			if (!isRamFound &&
			    ((value & ramMagicMask) == ramMagic)) {
				ramPtrBase = probe;
				isRamFound = true;
			}
		}

		if (isRamFound)
			break;
	}

	return ramPtrBase;
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
	uint8_t *ramPtrBase = nullptr;

	if (type_ == EmulatorType::MUPEN) {
		ramPtrBase = (uint8_t *)0x00505CB0;
		if (!probeRAMAddress(ramPtrBase))
			ramPtrBase = nullptr;
	}

	uint64_t parallelStart = 0;
	uint64_t parallelEnd = 0;
	if (type_ == EmulatorType::RETROARCH) {
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

			parallelStart = (uint64_t)mi.lpBaseOfDll;
			parallelEnd = parallelStart + mi.SizeOfImage;
		}
	}

	do {
		int offset = type_ == EmulatorType::MUPEN ? 0x20 : 0;
		MEMORY_BASIC_INFORMATION m;
		SIZE_T mbiSize = sizeof(m);
		SIZE_T result = VirtualQueryEx(process_, address, &m, mbiSize);
		if (address == (char *)m.BaseAddress + m.RegionSize ||
		    result == 0)
			break;

		DWORD prot = m.Protect & 0xff;
		if (prot == PAGE_EXECUTE_READWRITE ||
		    prot == PAGE_EXECUTE_WRITECOPY || prot == PAGE_READWRITE ||
		    prot == PAGE_WRITECOPY || prot == PAGE_READONLY) {
			uint8_t *ramPtrBaseCandidate =
				(uint8_t *)m.BaseAddress + offset;
			if (probeRAMAddress(ramPtrBaseCandidate)) {
				ramPtrBase = ramPtrBaseCandidate;
				break;
			}

			// Parallel: scan only large regions - we want to find g_rdram
			uint64_t regionSize = (uint64_t)m.RegionSize;
			uint64_t _address = (uint64_t)address;
			if (parallelStart <= _address &&
			    _address <= parallelEnd && regionSize >= 0x800000) {
				ramPtrBase = (uint8_t *)scanForRAM(
					address, m.RegionSize, 0x1000);
			}

			if (parallelStart != 0 && regionSize >= 0x800000) {
				ramPtrBase = (uint8_t *)scanForRAM(
					address, m.RegionSize, 0x10000);
			}

			// Modern mupen allocates a gigantic array with very strict alignment
			if (regionSize >= 0x100000000) {
				ramPtrBase = (uint8_t *)scanForRAM(
					address, 0x20000, 0x1000);
			}
		}
		address = (uint8_t *)m.BaseAddress + m.RegionSize;
	} while (!ramPtrBase && address <= MaxAddress);

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
