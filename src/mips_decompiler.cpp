#include "mips_decompiler.h"
#include "mips_converter.h"

#include <stdexcept>

namespace MIPS {
Instruction decodeInstruction(uint32_t inst)
{
	Instruction ret;

	if (inst == 0) {
		ret.cmd = CMD_NOP;
		return ret;
	}

	Op op = static_cast<Op>((inst >> 26) & 0x3F);

	if (op == OP_SPECIAL) {
		Funct funct = static_cast<Funct>(inst & 0x3F);
		ret.cmd = ToCmd(funct);
	} else if (op == OP_REGIMM) {
		FunctImm functImm = static_cast<FunctImm>((inst >> 16) & 0x1F);
		ret.cmd = ToCmd(functImm);
	} else if (op == OP_COP0) {
		Cop cop = static_cast<Cop>((inst >> 21) & 0x1F);

		if (cop != COP_MF && cop != COP_MT) {
			throw std::invalid_argument("Unknown COP instruction");
		}

		ret.cmd = ToCmd(cop, 0);
	} else if (op == OP_COP1) {
		throw std::invalid_argument("CoProcessor 1 is not supported");
	} else {
		ret.cmd = ToCmd(op);
	}

	Format format = ToFormat(ret.cmd);

	if (format & FORMAT_IMM) {
		ret.imm = static_cast<short>(inst & 0xFFFF);
	}

	if (format & FORMAT_OFF) {
		short off = static_cast<short>(inst & 0xFFFF);
		ret.off = (ExtendSign(off) << 2);
	}

	if (format & FORMAT_JUMP) {
		ret.jump = inst & 0x3FFFFFF;
	}

	if (format & FORMAT_REG_S) {
		ret.rs = static_cast<Register>((inst >> 21) & 0x1F);
	}

	if (format & FORMAT_REG_T) {
		ret.rt = static_cast<Register>((inst >> 16) & 0x1F);
	}

	if (format & FORMAT_REG_D) {
		ret.rd = static_cast<Register>((inst >> 11) & 0x1F);
	}

	if (format & FORMAT_REG_A) {
		ret.shift = static_cast<uint16_t>((inst >> 6) & 0x1F);
	}

	if (format & FORMAT_COP0_D) {
		ret.cop0 = static_cast<Cop0Registers>((inst >> 11) & 0x1F);
	}

	if (format & FORMAT_CACHE_T) {
		ret.cache = static_cast<CacheOp>((inst >> 16) & 0x1F);
	}

	return ret;
}
}