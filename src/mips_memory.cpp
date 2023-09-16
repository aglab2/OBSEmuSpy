#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "mips_memory.h"

namespace MIPS {
bool Memory::isValidStackIndex(uint32_t index) {
	return index < 0x4000;
}

bool Memory::isValidRamIndex(uint32_t index) {
	return index < ram_.size();
}

uint32_t Memory::read(uint32_t vAddr)
{
	uint32_t seg = vAddr >> 24;
	uint32_t off = vAddr & 0x00ffffff;
	off /= 4;

	if (seg == 0x80) {
		if (isValidRamIndex(off)) {
			return ram_[off];
		} else {
			throw std::runtime_error(
				"RAM index out of bounds");
		}
	}

	if (seg == 0x81) {
		if (isValidStackIndex(off)) {
			return stack_[off];
		} else {
			throw std::runtime_error(
				"Stack index out of bounds");
		}
	}

	throw std::runtime_error("Unknown segment " +
					std::to_string(seg));
}

void Memory::write(int vAddr, uint8_t val, int dataOff)
{
	write(static_cast<uint32_t>(vAddr), val, dataOff);
}

void Memory::write(int vAddr, uint16_t val, int dataOff)
{
	write(static_cast<uint32_t>(vAddr), val, dataOff);
}

void Memory::write(int vAddr, uint32_t val)
{
	write(static_cast<uint32_t>(vAddr), val);
}

void Memory::write(uint32_t vAddr, uint8_t val, int dataOff)
{
	uint32_t seg = vAddr >> 24;
	uint32_t off = vAddr & 0x00ffffff;
	off /= 4;

	if (seg == 0x80) {
		if (isValidRamIndex(off)) {
			// int data = ram_[off];
			// data &= (0xff << (24 - 8 * dataOff));
			// data |= (val << (24 - 8 * dataOff));
			// ram_[off] = data;
			return;
		} else {
			throw std::runtime_error(
				"RAM index out of bounds");
		}
	}

	if (seg == 0x81) {
		if (isValidStackIndex(off)) {
			uint32_t data = stack_[off];
			data &= (0xffU << (24 - 8 * dataOff));
			data |= (static_cast<uint32_t>(val)
					<< (24 - 8 * dataOff));
			stack_[off] = data;
			return;
		} else {
			throw std::runtime_error(
				"Stack index out of bounds");
		}
	}

	throw std::runtime_error("Unknown segment " +
					std::to_string(seg));
}

void Memory::write(uint32_t vAddr, uint16_t val, int dataOff)
{
	uint32_t seg = vAddr >> 24;
	uint32_t off = vAddr & 0x00ffffff;
	off /= 4;

	if (seg == 0x80) {
		if (isValidRamIndex(off)) {
			// int data = ram_[off];
			// data &= (0xff << (16 - 16 * dataOff));
			// data |= (val << (16 - 16 * dataOff));
			// ram_[off] = data;
			return;
		} else {
			throw std::runtime_error(
				"RAM index out of bounds");
		}
	}

	if (seg == 0x81) {
		if (isValidStackIndex(off)) {
			uint32_t data = stack_[off];
			data &= (0xffU << (16 - 16 * dataOff));
			data |= (static_cast<uint32_t>(val)
					<< (16 - 16 * dataOff));
			stack_[off] = data;
			return;
		} else {
			throw std::runtime_error(
				"Stack index out of bounds");
		}
	}

	throw std::runtime_error("Unknown segment " +
					std::to_string(seg));
}

void Memory::write(uint32_t vAddr, uint32_t val)
{
	uint32_t seg = vAddr >> 24;
	uint32_t off = vAddr & 0x00ffffff;
	off /= 4;

	if (seg == 0x80) {
		if (isValidRamIndex(off)) {
			// ram_[off] = val;
			return;
		} else {
			throw std::runtime_error(
				"RAM index out of bounds");
		}
	}

	if (seg == 0x81) {
		if (isValidStackIndex(off)) {
			stack_[off] = val;
			return;
		} else {
			throw std::runtime_error(
				"Stack index out of bounds");
		}
	}

	throw std::runtime_error("Unknown segment " +
					std::to_string(seg));
}
}
