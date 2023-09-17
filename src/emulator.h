#pragma once

#include "mips_analyzer.h"
#include "winpp.h"

#include <condition_variable>
#include <mutex>
#include <thread>

struct Emulator {
public:
	Emulator();
	~Emulator();

	Emulator &operator==(const Emulator &) = delete;
	Emulator(const Emulator &) = delete;

	int32_t getInputs() const
	{
		return inputs_.load(std::memory_order_relaxed);
	}

private:
	void work();

	void searchProcess();
	void scanProcessRAM();
	int32_t feedInputs();

	void markProcessDead();
	void markRAMDead();

	bool probeRAMAddress(void *);

	std::atomic<int32_t> inputs_;

	enum EmulatorType {
		UNKNOWN,
		PJ64,
		RETROARCH,
	};

	uint32_t pid_; // diagnostics only...
	WinHandle process_;
	EmulatorType type_ = EmulatorType::UNKNOWN;
	BOOL processIs64Bit_ = false;
	uint8_t *ramPtrBase_ = nullptr;
	std::optional<MIPS::AnalyzeResult> analyzeResult_;
	int msToWait_ = 1;

	bool running_ = true;
	std::condition_variable cv_;
	std::mutex mutex_;
	std::thread thread_;
};
