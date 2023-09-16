#pragma once

#include <stdint.h>

#include <optional>
#include <vector>

namespace MIPS {

struct AnalyzeResult {
	int interpretedInstructionsOffset;
	std::vector<uint32_t> interpretedInstructions;
	int gControllerPads;
};

std::optional<AnalyzeResult> analyze(const std::vector<uint32_t> &mem);
}
