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

		HMODULE mod;
		DWORD needed;
		if (!EnumProcessModules(process, &mod, sizeof(HMODULE),
					&needed))
			continue;

		char name[MAX_PATH];
		name[0] = 0;
		GetModuleBaseNameA(process, mod, name,
				   sizeof(name) / sizeof(TCHAR));

		std::string nameStr{name};
		std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(),
			       [](unsigned char c) { return std::tolower(c); });

		if (nameStr != "project64.exe" && nameStr != "retroarch.exe")
			continue;

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
	int offset = 0;
	const uint32_t ramMagic = 0x3C1A8000;
	const uint32_t ramMagicMask = 0xfffff000;
	uint8_t *ramPtrBase = nullptr;
	do {
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
			uint32_t value;
			uint8_t *ramPtrBaseCandidate =
				(uint8_t *)m.BaseAddress + offset;
			BOOL readSuccess = ReadProcessMemory(
				process_, ramPtrBaseCandidate, &value,
				sizeof(value), nullptr);
			if (readSuccess) {
				if ((value & ramMagicMask) == ramMagic) {
					ramPtrBase = ramPtrBaseCandidate;
					break;
				}
			}

			// scan only large regions - we want to find g_rdram
			/*
			ulong regionSize = (ulong)m.RegionSize;
			if (parallelStart <= address &&
			    address <= parallelEnd && regionSize >= 0x800000) {
				// g_rdram is aligned to 0x1000
				ulong maxCnt = (ulong)m.RegionSize / 0x1000;
				for (ulong num = 0; num < maxCnt; num++) {
					readSuccess = process.ReadValue(
						new IntPtr(
							(long)(address +
							       num * 0x1000)),
						out value);
					if (readSuccess) {
						if (!isRamFound &&
						    ((value & ramMagicMask) ==
						     ramMagic)) {
							ramPtrBase =
								address +
								num * 0x1000;
							isRamFound = true;
						}
					}

					if (isRamFound)
						break;
				}
			}*/
		}

		address = (uint8_t *)m.BaseAddress + m.RegionSize;
	} while (address <= MaxAddress);

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
