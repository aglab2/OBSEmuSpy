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
		if (!pj64 && !retroarch)
			continue;

		type_ = pj64 ? EmulatorType::PJ64 : EmulatorType::RETROARCH;
		pid_ = pid;
		process_ = std::move(process);
		IsWow64Process(process_, &processIs64Bit_);
		break;
	}
}

bool Emulator::probeRAMAddress(void *ramPtrBaseCandidate)
{
	const uint32_t ramMagic = 0x3C1A8000;
	const uint32_t ramMagicMask = 0xfffff000;

	uint32_t value;
	if (!ReadProcessMemory(process_, ramPtrBaseCandidate, &value,
			       sizeof(value), nullptr))
		return false;

	if ((value & ramMagicMask) == ramMagic) {
		return true;
	}

	return false;
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
