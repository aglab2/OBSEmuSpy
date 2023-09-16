#pragma once

#include "mips_instruction.h"

#include <map>

namespace MIPS {
static Cmd ToCmd(Op op)
{
	return (Cmd)(CMD_IMM | static_cast<Cmd>(op));
}

static Cmd ToCmd(Funct funct)
{
	return (Cmd)(CMD_REG | static_cast<Cmd>(funct));
}

static Cmd ToCmd(FunctImm functImm)
{
	return (Cmd)(CMD_REGIMM | static_cast<Cmd>(functImm));
}

static Cmd ToCmd(Cop cop, int num)
{
	return (Cmd)((num == 0 ? CMD_COP0 : CMD_COP1) | static_cast<Cmd>(cop));
}

static Op ToOp(Cmd cmd)
{
	return static_cast<Op>(static_cast<int>(cmd) & 0xffffff);
}

static Funct ToFunct(Cmd cmd)
{
	return static_cast<Funct>(static_cast<int>(cmd) & 0xffffff);
}

static FunctImm ToFunctImm(Cmd cmd)
{
	return static_cast<FunctImm>(static_cast<int>(cmd) & 0xffffff);
}

Format ToFormat(Cmd cmd);

uint32_t ToUInt(Instruction inst);

static uint32_t Extract(uint32_t inst, int off, int bits)
{
	uint32_t data = inst >> off;
	uint32_t mask = 1U << bits;
	return data & (mask - 1);
}

static int ExtendZero(int16_t val)
{
	uint16_t uval = static_cast<uint16_t>(val);
	return static_cast<int>(uval);
}

static int ExtendSign(int16_t val)
{
	return static_cast<int>(val);
}

}
