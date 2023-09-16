#pragma once

#include "mips_instruction.h"

#include <stdint.h>

namespace MIPS {
Instruction decodeInstruction(uint32_t inst);
}
