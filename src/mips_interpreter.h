#pragma once

#include "mips_types.h"
#include "mips_instruction.h"
#include "mips_memory.h"

#include <stdint.h>

#include <map>

namespace MIPS {

class Interpreter {
public:
	int32_t gpr[32]{};
	int32_t lo{};
	int32_t hi{};
	uint32_t pc{};
	Memory memory;

	Interpreter(const std::vector<uint32_t> &ram) : memory(ram) {}

	using Performer = void (Interpreter::*)(const Instruction &);

	void execute(const Instruction &inst);
	std::optional<Instruction> getInstruction();

private:
	static const std::map<Cmd, Interpreter::Performer> sCmdToFunc;

	void Add(const Instruction &inst);
	void AddI(const Instruction &inst);
	void And(const Instruction &inst);
	void AndI(const Instruction &inst);
	void Div(const Instruction &inst);
	void DivU(const Instruction &inst);
	void LB(const Instruction &inst);
	void LBU(const Instruction &inst);
	void LH(const Instruction &inst);
	void LHU(const Instruction &inst);
	void LUI(const Instruction &inst);
	void LW(const Instruction &inst);
	void MFHI(const Instruction &inst);
	void MFLO(const Instruction &inst);
	void MTHI(const Instruction &inst);
	void MTLO(const Instruction &inst);
	void Mult(const Instruction &inst);
	void MultU(const Instruction &inst);
	void NOr(const Instruction &inst);
	void Or(const Instruction &inst);
	void OrI(const Instruction &inst);
	void SB(const Instruction &inst);
	void SH(const Instruction &inst);
	void SLL(const Instruction &inst);
	void SLLV(const Instruction &inst);
	void SLT(const Instruction &inst);
	void SLTI(const Instruction &inst);
	void SLTIU(const Instruction &inst);
	void SLTU(const Instruction &inst);
	void SRA(const Instruction &inst);
	void SRAV(const Instruction &inst);
	void SRL(const Instruction &inst);
	void SRLV(const Instruction &inst);
	void Sub(const Instruction &inst);
	void SubU(const Instruction &inst);
	void SW(const Instruction &inst);
	void Xor(const Instruction &inst);
	void XorI(const Instruction &inst);
};
}

