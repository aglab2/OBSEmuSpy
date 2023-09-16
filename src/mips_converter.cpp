#include "mips_converter.h"

#include <stdexcept>

namespace MIPS {
static const std::map<Cmd, Format> sCmdToFormat{
	{CMD_NOP, FORMAT_NONE},

	{CMD_ADDI, FORMAT_REGIMM_ST},
	{CMD_ADDIU, FORMAT_REGIMM_ST},
	{CMD_ANDI, FORMAT_REGIMM_ST},
	{CMD_BEQ, FORMAT_REGOFF_ST},
	{CMD_BEQL, FORMAT_REGOFF_ST},
	{CMD_BGTZ, FORMAT_REGOFF_S},
	{CMD_BGTZL, FORMAT_REGOFF_S},
	{CMD_BLEZ, FORMAT_REGOFF_S},
	{CMD_BLEZL, FORMAT_REGOFF_S},
	{CMD_BNE, FORMAT_REGOFF_ST},
	{CMD_BNEL, FORMAT_REGOFF_ST},
	{CMD_DADDI, FORMAT_REGIMM_ST},
	{CMD_DADDIU, FORMAT_REGIMM_ST},
	{CMD_J, FORMAT_JUMP},
	{CMD_JAL, FORMAT_JUMP},
	{CMD_LB, FORMAT_REGOFF_ST},
	{CMD_LBU, FORMAT_REGOFF_ST},
	{CMD_LD, FORMAT_REGOFF_ST},
	{CMD_LDL, FORMAT_REGOFF_ST},
	{CMD_LDR, FORMAT_REGOFF_ST},
	{CMD_LH, FORMAT_REGOFF_ST},
	{CMD_LHU, FORMAT_REGOFF_ST},
	{CMD_LL, FORMAT_REGOFF_ST},
	{CMD_LLD, FORMAT_REGOFF_ST},
	{CMD_LUI, FORMAT_REGIMM_T},
	{CMD_LW, FORMAT_REGOFF_ST},
	{CMD_LWL, FORMAT_REGOFF_ST},
	{CMD_LWR, FORMAT_REGOFF_ST},
	{CMD_LWU, FORMAT_REGOFF_ST},
	{CMD_ORI, FORMAT_REGIMM_ST},
	{CMD_SB, FORMAT_REGOFF_ST},
	{CMD_SC, FORMAT_REGOFF_ST},
	{CMD_SCD, FORMAT_REGOFF_ST},
	{CMD_SD, FORMAT_REGOFF_ST},
	{CMD_SDL, FORMAT_REGOFF_ST},
	{CMD_SDR, FORMAT_REGOFF_ST},
	{CMD_SH, FORMAT_REGOFF_ST},
	{CMD_SLTI, FORMAT_REGIMM_ST},
	{CMD_SLTIU, FORMAT_REGIMM_ST},
	{CMD_SW, FORMAT_REGOFF_ST},
	{CMD_SWL, FORMAT_REGOFF_ST},
	{CMD_SWR, FORMAT_REGOFF_ST},
	{CMD_XORI, FORMAT_REGIMM_ST},

	{CMD_ADD, FORMAT_REG_STD},
	{CMD_ADDU, FORMAT_REG_STD},
	{CMD_AND, FORMAT_REG_STD},
	//            { CMD_BREAK, FORMAT_REG_STD },
	{CMD_DADD, FORMAT_REG_STD},
	{CMD_DADDU, FORMAT_REG_STD},
	{CMD_DDIV, FORMAT_REG_ST},
	{CMD_DDIVU, FORMAT_REG_ST},
	{CMD_DIV, FORMAT_REG_ST},
	{CMD_DIVU, FORMAT_REG_ST},
	{CMD_DMULT, FORMAT_REG_ST},
	{CMD_DMULTU, FORMAT_REG_ST},
	{CMD_DSLL, FORMAT_REG_TDA},
	{CMD_DSLL32, FORMAT_REG_TDA},
	{CMD_DSLLV, FORMAT_REG_STD},
	{CMD_DSRA, FORMAT_REG_TDA},
	{CMD_DSRA32, FORMAT_REG_TDA},
	{CMD_DSRAV, FORMAT_REG_STD},
	{CMD_DSRL, FORMAT_REG_TDA},
	{CMD_DSRL32, FORMAT_REG_TDA},
	{CMD_DSRLV, FORMAT_REG_STD},
	{CMD_DSUB, FORMAT_REG_STD},
	{CMD_DSUBU, FORMAT_REG_STD},
	{CMD_JALR, FORMAT_REG_SD},
	{CMD_JR, FORMAT_REG_S},
	{CMD_MFHI, FORMAT_REG_D},
	{CMD_MFLO, FORMAT_REG_D},
	{CMD_MTHI, FORMAT_REG_S},
	{CMD_MTLO, FORMAT_REG_S},
	{CMD_MULT, FORMAT_REG_ST},
	{CMD_MULTU, FORMAT_REG_ST},
	{CMD_NOR, FORMAT_REG_STD},
	{CMD_OR, FORMAT_REG_STD},
	{CMD_SLL, FORMAT_REG_TDA},
	{CMD_SLLV, FORMAT_REG_STD},
	{CMD_SLT, FORMAT_REG_STD},
	{CMD_SLTU, FORMAT_REG_STD},
	{CMD_SRA, FORMAT_REG_TDA},
	{CMD_SRAV, FORMAT_REG_STD},
	{CMD_SRL, FORMAT_REG_TDA},
	{CMD_SRLV, FORMAT_REG_STD},
	{CMD_SUB, FORMAT_REG_STD},
	{CMD_SUBU, FORMAT_REG_STD},
	{CMD_SYNC, FORMAT_REG_A},
	//            { CMD_SYSCALL, FORMAT_REG_STD },
	{CMD_XOR, FORMAT_REG_STD},

	{CMD_BGEZ, FORMAT_REGOFF_S},
	{CMD_BGEZAL, FORMAT_REGOFF_S},
	{CMD_BGEZALL, FORMAT_REGOFF_S},
	{CMD_BGEZL, FORMAT_REGOFF_S},
	{CMD_BLTZ, FORMAT_REGOFF_S},
	{CMD_BLTZAL, FORMAT_REGOFF_S},
	{CMD_BLTZALL, FORMAT_REGOFF_S},
	{CMD_BLTZL, FORMAT_REGOFF_S},

	{CMD_MTC0, FORMAT_REG_COP0},
	{CMD_MFC0, FORMAT_REG_COP0},

	{CMD_CACHE, FORMAT_REGOFF_CACHE},
};

Format ToFormat(Cmd cmd)
{
	auto it = sCmdToFormat.find(cmd);
	if (it == sCmdToFormat.end())
		throw std::logic_error("invalid cmd");

	return it->second;
}

uint32_t ToUInt(Instruction inst)
{
	if (inst.cmd == CMD_NOP)
		return 0;

	Format format = ToFormat(inst.cmd);

	uint32_t ret = 0;

	if (static_cast<int>(format) & static_cast<int>(FORMAT_IMM)) {
		ret |= static_cast<uint16_t>(*inst.imm);
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_OFF)) {
		ret |= static_cast<uint16_t>(*inst.off >> 2);
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_JUMP)) {
		ret |= *inst.jump >> 2;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_REG_S)) {
		ret |= static_cast<uint32_t>(*inst.rs) << 21;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_REG_T)) {
		ret |= static_cast<uint32_t>(*inst.rt) << 16;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_REG_D)) {
		ret |= static_cast<uint32_t>(*inst.rd) << 11;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_REG_A)) {
		ret |= static_cast<uint32_t>(*inst.shift) << 6;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_COP0_D)) {
		ret |= static_cast<uint32_t>(*inst.cop0) << 11;
	}
	if (static_cast<int>(format) & static_cast<int>(FORMAT_CACHE_T)) {
		ret |= static_cast<uint32_t>(*inst.cache) << 16;
	}

	Cmd cmd = inst.cmd;
	uint32_t cmdVal = static_cast<uint32_t>(cmd) & 0b111111;
	if ((cmd & CMD_REGIMM) == CMD_REGIMM) {
		uint32_t regimm = 0b000001;
		ret |= regimm << 26;
		ret |= cmdVal << 16;
	} else if (cmd & CMD_REG) {
		ret |= cmdVal;
	} else if (cmd & CMD_IMM) {
		ret |= cmdVal << 26;
	} else if (cmd & CMD_COP0) {
		uint32_t cop0 = 0b010000;
		ret |= cop0 << 26;
		ret |= cmdVal << 21;
	} else {
		throw std::runtime_error("Bad Cmd passed!");
	}

	return ret;
}
}
