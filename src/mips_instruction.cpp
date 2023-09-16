#include "mips_instruction.h"

#include <iomanip>
#include <sstream>

namespace MIPS
{
std::string intToHexString(uint32_t value, int width = 0)
{
	std::ostringstream stream;
	stream << std::hex << std::uppercase << std::setfill('0')
	       << std::setw(width) << value;
	return stream.str();
}

std::string Instruction::toString()
{
	std::string output = "";

	output += static_cast<int>(cmd); // Convert enum to int

	if (rs.has_value() && rt.has_value() && off.has_value()) {
		if (cmd != CMD_BEQ && cmd != CMD_BEQL && cmd != CMD_BNE &&
		    cmd != CMD_BNEL) {
			output += " " + std::to_string(rt.value()) + ", 0x" +
				  intToHexString(off.value()) + "(" +
				  std::to_string(rs.value()) + ")";
		}
	}

	std::string sep = " ";
	auto append = [&](const std::string &app) {
		output += sep + app;
		sep = ", ";
	};

	if (cache.has_value()) {
		append("(" + std::to_string(static_cast<int>(cache.value())) +
		       ")");
	}
	if (rd.has_value()) {
		append(std::to_string(static_cast<int>(rd.value())));
	}
	if (imm.has_value()) {
		if (rt.has_value()) {
			append(std::to_string(static_cast<int>(rt.value())));
		}
		if (rs.has_value()) {
			append(std::to_string(static_cast<int>(rs.value())));
		}
	} else {
		if (rs.has_value()) {
			append(std::to_string(static_cast<int>(rs.value())));
		}
		if (rt.has_value()) {
			append(std::to_string(static_cast<int>(rt.value())));
		}
	}
	if (off.has_value()) {
		append("0x" + intToHexString(off.value(), 4));
	}
	if (jump.has_value()) {
		append("0x" + intToHexString(jump.value(), 8));
	}
	if (imm.has_value()) {
		append("0x" + intToHexString(imm.value(), 4));
	}
	if (shift.has_value()) {
		append(std::to_string(shift.value()));
	}
	if (cop0.has_value()) {
		append(std::to_string(static_cast<int>(cop0.value())));
	}

	return output;
}
}
