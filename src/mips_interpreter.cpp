#include "mips_interpreter.h"

#include "mips_decompiler.h"

namespace MIPS {
void Interpreter::Add(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rs + rt;
	}
}

void Interpreter::AddI(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t imm = static_cast<int>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = rs + imm;
	}
}

void Interpreter::And(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rs & rt;
	}
}

void Interpreter::AndI(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t imm = static_cast<int>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = rs & imm;
	}
}

void Interpreter::Div(const Instruction &inst)
{
	if (inst.rs && inst.rt) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		if (0 == rt)
			throw std::logic_error("Bad div");

		lo = rs % rt;
		hi = rs / rt;
	}
}

void Interpreter::DivU(const Instruction &inst)
{
	if (inst.rs && inst.rt) {
		uint32_t rsv =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rs)]);
		uint32_t rtv =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		if (0 == rtv)
			throw std::logic_error("Bad div");

		lo = rsv % rtv;
		hi = rsv / rtv;
	}
}

void Interpreter::LB(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t dataPos = vAddr & 0x3;
		uint32_t pAddr = memory.read(vAddr);
		gpr[static_cast<int>(*inst.rt)] =
			static_cast<int8_t>(pAddr >> (24 - dataPos * 8));
	}
}

void Interpreter::LBU(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t dataPos = vAddr & 0x3;
		uint32_t pAddr = memory.read(vAddr);
		gpr[static_cast<int>(*inst.rt)] =
			static_cast<uint8_t>(pAddr >> (24 - dataPos * 8));
	}
}

void Interpreter::LH(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t shortPos = vAddr & 0x1;
		uint32_t pAddr = memory.read(vAddr);
		gpr[static_cast<int>(*inst.rt)] =
			static_cast<int16_t>(pAddr >> (16 - shortPos * 16));
	}
}

void Interpreter::LHU(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t shortPos = vAddr & 0x1;
		uint32_t pAddr = memory.read(vAddr);
		gpr[static_cast<int>(*inst.rt)] =
			static_cast<uint16_t>(pAddr >> (16 - shortPos * 16));
	}
}

void Interpreter::LUI(const Instruction &inst)
{
	if (inst.rt && inst.imm) {
		int32_t imm = static_cast<int>(*inst.imm);
		gpr[static_cast<int>(*inst.rt)] = imm << 16;
	}
}

void Interpreter::LW(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		gpr[static_cast<int>(*inst.rt)] = memory.read(vAddr);
	}
}

void Interpreter::MFHI(const Instruction &inst)
{
	if (inst.rd) {
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = hi;
	}
}

void Interpreter::MFLO(const Instruction &inst)
{
	if (inst.rd) {
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = lo;
	}
}

void Interpreter::MTHI(const Instruction &inst)
{
	if (inst.rs) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		hi = rs;
	}
}

void Interpreter::MTLO(const Instruction &inst)
{
	if (inst.rs) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		lo = rs;
	}
}

void Interpreter::Mult(const Instruction &inst)
{
	if (inst.rs && inst.rt) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int64_t val =
			static_cast<int64_t>(rs) * static_cast<int64_t>(rt);
		lo = static_cast<int32_t>(val);
		hi = static_cast<int32_t>(val >> 32);
	}
}

void Interpreter::MultU(const Instruction &inst)
{
	if (inst.rs && inst.rt) {
		uint32_t rs =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rs)]);
		uint32_t rt =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		uint64_t val =
			static_cast<uint64_t>(rs) * static_cast<uint64_t>(rt);
		lo = static_cast<int32_t>(val);
		hi = static_cast<int32_t>(val >> 32);
	}
}

void Interpreter::NOr(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = ~(rs | rt);
	}
}

void Interpreter::Or(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rs | rt;
	}
}

void Interpreter::OrI(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t imm = static_cast<int>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = rs | imm;
	}
}

void Interpreter::SB(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t dataPos = vAddr & 0x3;
		memory.write(vAddr, static_cast<uint8_t>(rt), dataPos);
	}
}

void Interpreter::SH(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		int32_t shortPos = vAddr & 0x1;
		memory.write(vAddr, static_cast<uint16_t>(rt), shortPos);
	}
}

void Interpreter::SLL(const Instruction &inst)
{
	if (inst.rt && inst.rd && inst.shift) {
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		int32_t sa = static_cast<int>(*inst.shift);
		gpr[rd] = rt << sa;
	}
}

void Interpreter::SLLV(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rt << rs;
	}
}

void Interpreter::SLT(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = (rs < rt) ? 1 : 0;
	}
}

void Interpreter::SLTI(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t imm = static_cast<int>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = (rs < imm) ? 1 : 0;
	}
}

void Interpreter::SLTIU(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		uint32_t rs =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rs)]);
		uint32_t imm = static_cast<uint32_t>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = (rs < imm) ? 1 : 0;
	}
}

void Interpreter::SLTU(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		uint32_t rs =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rs)]);
		uint32_t rt =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = (rs < rt) ? 1 : 0;
	}
}

void Interpreter::SRA(const Instruction &inst)
{
	if (inst.rt && inst.rd && inst.shift) {
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		int32_t sa = static_cast<int>(*inst.shift);
		gpr[rd] = rt >> sa;
	}
}

void Interpreter::SRAV(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rt >> rs;
	}
}

void Interpreter::SRL(const Instruction &inst)
{
	if (inst.rt && inst.rd && inst.shift) {
		uint32_t rt =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		int32_t rd = static_cast<int>(*inst.rd);
		int32_t sa = static_cast<int>(*inst.shift);
		gpr[rd] = static_cast<int32_t>(rt >> sa);
	}
}

void Interpreter::SRLV(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		uint32_t rt =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = static_cast<int32_t>(rt >> rs);
	}
}

void Interpreter::Sub(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rs - rt;
	}
}

void Interpreter::SubU(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		uint32_t rs =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rs)]);
		uint32_t rt =
			static_cast<uint32_t>(gpr[static_cast<int>(*inst.rt)]);
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = static_cast<int32_t>(rs - rt);
	}
}

void Interpreter::SW(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.off) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t off = static_cast<int>(*inst.off);
		int32_t vAddr = off + rs;
		memory.write(vAddr, static_cast<uint32_t>(rt));
	}
}

void Interpreter::Xor(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.rd) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t rt = gpr[static_cast<int>(*inst.rt)];
		int32_t rd = static_cast<int>(*inst.rd);
		gpr[rd] = rs ^ rt;
	}
}

void Interpreter::XorI(const Instruction &inst)
{
	if (inst.rs && inst.rt && inst.imm) {
		int32_t rs = gpr[static_cast<int>(*inst.rs)];
		int32_t imm = static_cast<int>(*inst.imm);
		int32_t rt = static_cast<int>(*inst.rt);
		gpr[rt] = rs ^ imm;
	}
}

const std::map<Cmd, Interpreter::Performer> Interpreter::sCmdToFunc{
	{CMD_ADD, &Interpreter::Add},     {CMD_ADDU, &Interpreter::Add},
	{CMD_ADDI, &Interpreter::AddI},   {CMD_ADDIU, &Interpreter::AddI},
	{CMD_AND, &Interpreter::And},     {CMD_ANDI, &Interpreter::AndI},
	{CMD_DIV, &Interpreter::Div},     {CMD_DIVU, &Interpreter::DivU},
	{CMD_LB, &Interpreter::LB},       {CMD_LBU, &Interpreter::LBU},
	{CMD_LH, &Interpreter::LH},       {CMD_LHU, &Interpreter::LHU},
	{CMD_LUI, &Interpreter::LUI},     {CMD_LW, &Interpreter::LW},
	{CMD_LWU, &Interpreter::LW},      {CMD_MFHI, &Interpreter::MFHI},
	{CMD_MFLO, &Interpreter::MFLO},   {CMD_MTHI, &Interpreter::MTHI},
	{CMD_MTLO, &Interpreter::MTLO},   {CMD_MULT, &Interpreter::Mult},
	{CMD_MULTU, &Interpreter::MultU}, {CMD_NOR, &Interpreter::NOr},
	{CMD_OR, &Interpreter::Or},       {CMD_ORI, &Interpreter::OrI},
	{CMD_SB, &Interpreter::SB},       {CMD_SH, &Interpreter::SH},
	{CMD_SLLV, &Interpreter::SLLV},   {CMD_SLT, &Interpreter::SLT},
	{CMD_SLTI, &Interpreter::SLTI},   {CMD_SLTIU, &Interpreter::SLTIU},
	{CMD_SLTU, &Interpreter::SLTU},   {CMD_SRA, &Interpreter::SRA},
	{CMD_SRAV, &Interpreter::SRAV},   {CMD_SRL, &Interpreter::SRL},
	{CMD_SRLV, &Interpreter::SRLV},   {CMD_SUB, &Interpreter::Sub},
	{CMD_SUBU, &Interpreter::Sub},    {CMD_SW, &Interpreter::SW},
	{CMD_XOR, &Interpreter::Xor},     {CMD_XORI, &Interpreter::XorI},
};

void Interpreter::execute(const Instruction &inst)
try {
	auto perform = sCmdToFunc.find(inst.cmd);
	if (perform != sCmdToFunc.end()) {
		(this->*(perform->second))(inst);
	}
} catch (...) {
}

std::optional<Instruction> Interpreter::getInstruction()
{
	uint32_t cmd = memory.read(static_cast<int>(pc));
	pc += 4;

	try {
		return decodeInstruction(cmd);
	} catch (...) {
		return std::nullopt;
	}
}
}