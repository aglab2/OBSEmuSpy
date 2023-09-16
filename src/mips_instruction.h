#pragma once

#include "mips_types.h"
#include <optional>
#include <string>

namespace MIPS {
struct Instruction {
	Cmd cmd;
	std::optional<Register> rs;
	std::optional<Register> rt;
	std::optional<Register> rd;
	std::optional<int> shift;
	std::optional<short> imm;
	std::optional<int> off;
	std::optional<uint32_t> jump;
	std::optional<Cop0Registers> cop0;
	std::optional<CacheOp> cache;

	std::string toString();
};
}
