#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <stdint.h>

namespace MIPS {
class Memory {
private:
	uint32_t x;
	const std::vector<uint32_t> &ram_;
	std::vector<uint32_t> stack_;

	bool isValidStackIndex(uint32_t index);
	bool isValidRamIndex(uint32_t index);

public:
	Memory(const std::vector<uint32_t> &ram) : ram_(ram), stack_(0x4000, 0)
	{
	}

	uint32_t read(uint32_t vAddr);

	void write(int vAddr, uint8_t val, int dataOff);
	void write(int vAddr, uint16_t val, int dataOff);
	void write(int vAddr, uint32_t val);

	void write(uint32_t vAddr, uint8_t val, int dataOff);
	void write(uint32_t vAddr, uint16_t val, int dataOff);
	void write(uint32_t vAddr, uint32_t val);
};
}
