#include "mips_analyzer.h"
#include "mips_converter.h"
#include "mips_decompiler.h"
#include "mips_instruction.h"
#include "mips_interpreter.h"

#include <stdint.h>

#include <set>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

namespace MIPS {

struct MaskPair {
	uint32_t val;
	uint32_t mask;

	MaskPair(uint32_t val, uint32_t mask) : val(val), mask(mask) {}

	std::string toString() const
	{
		std::ostringstream stream;
		stream << std::hex << std::showbase << "0x" << val << ", 0x"
		       << mask;
		return stream.str();
	}
};

static std::vector<int>
IndicesOf(const std::vector<uint32_t> &arrayToSearchThrough,
	  const MaskPair *patternToFind, size_t patternToFindSize)
{
	std::vector<int> ret;

	if (patternToFindSize > arrayToSearchThrough.size())
		return ret;

	for (size_t i = 0; i <= arrayToSearchThrough.size() - patternToFindSize;
	     ++i) {
		bool found = true;
		for (size_t j = 0; j < patternToFindSize; ++j) {
			uint32_t data = arrayToSearchThrough[i + j];
			uint32_t expectedAfterMasking = patternToFind[j].val;
			uint32_t mask = patternToFind[j].mask;
			uint32_t maskedData = data & (~mask);
			uint32_t leftoverData = data & mask;

			if (maskedData != expectedAfterMasking ||
			    (mask != 0 && leftoverData == 0)) {
				found = false;
				break;
			}
		}

		if (found) {
			ret.push_back(static_cast<int>(i));
		}
	}

	return ret;
}

static std::vector<int>
IndicesOf(const std::vector<uint32_t> &arrayToSearchThrough,
	  const uint32_t *patternToFind, size_t patternToFindSize)
{
	std::vector<int> ret;

	if (patternToFindSize > arrayToSearchThrough.size())
		return ret;

	for (size_t i = 0; i <= arrayToSearchThrough.size() - patternToFindSize;
	     ++i) {
		bool found = true;
		for (size_t j = 0; j < patternToFindSize; ++j) {
			if (arrayToSearchThrough[i + j] != patternToFind[j]) {
				found = false;
				break;
			}
		}

		if (found) {
			ret.push_back(static_cast<int>(i));
		}
	}

	return ret;
}

static std::vector<int>
FindAll(const std::vector<uint32_t> &arrayToSearchThrough, uint32_t val)
{
	std::vector<int> list;

	for (size_t i = 0; i < arrayToSearchThrough.size(); ++i) {
		if (arrayToSearchThrough[i] == val) {
			list.push_back(static_cast<int>(i));
		}
	}

	return list;
}

static const uint32_t OsGetCount[] = {0x40024800, 0x03E00008, 0x00000000};

static const struct MaskPair OsDisableInt[] = {
	{0x40006000,
	 0x001F0000}, // MFC0     T0, Status              MFC0     __, Status
	{0x2400FFFE,
	 0x001F0000}, // ADDIU    AT, R0, 0xFFFE          ADDIU    __, R0, 0xFFFE
	{0x00000024,
	 0x03FFF800}, // AND      T1, T0, AT              AND      __, __, __
	{0x40806000,
	 0x001F0000}, // MTC0     T1, Status              MTC0     __, Status
	{0x30020001,
	 0x03E00000}, // ANDI     V0, T0, 0x0001          ANDI     V0, __, 0x0001
};

static const struct MaskPair OsRestoreInt[] = {
	{0x40006000,
	 0x001F0000}, // MFC0     T0, Status              MFC0     __, Status
	{0x00040025,
	 0x03E0F800}, // OR       T0, T0, A0              OR       __, __, A0
	{0x40806000,
	 0x001F0000}, // MTC0     T0, Status              MTC0     __, Status
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x03E00008,
	 0x00000000}, // JR       RA                      JR       RA
	{0x00000000, 0x00000000}, // NOP                              NOP
};

static const struct MaskPair OsWritebackDCache[] = {
	{0x00000023,
	 0x03FFF800}, // SUBU     T0, T0, T2              SUBU     __, __, __
	{0xBC190000,
	 0x03E00000}, // CACHE    (D, IIndexLoadData), T0, 0x0000 CACHE    (D, IIndexLoadData), __, 0x0000
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T0, T1              SLTU     __, __, __
	{0x1400FFFD,
	 0x03E00000}, // BNE      R0, 0xFFFFFFF4(AT)      BNE      R0, 0xFFFFFFF4(__)
	{0x24000010,
	 0x03FF0000}, // ADDIU    T0, T0, 0x0010          ADDIU    __, __, 0x0010
	{0x03E00008,
	 0x00000000}, // JR       RA                      JR       RA
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x3C008000,
	 0x001F0000}, // LUI      T0, 0x8000              LUI      __, 0x8000
	{0x00000021,
	 0x03FFF800}, // ADDU     T1, T0, T3              ADDU     __, __, __
	{0x2400FFF0,
	 0x03FF0000}, // ADDIU    T1, T1, 0xFFF0          ADDIU    __, __, 0xFFF0
	{0xBC010000,
	 0x03E00000}, // CACHE    (D), T0, 0x0000         CACHE    (D), __, 0x0000
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T0, T1              SLTU     __, __, __
	{0x1400FFFD,
	 0x03E00000}, // BNE      R0, 0xFFFFFFF4(AT)      BNE      R0, 0xFFFFFFF4(__)
	{0x24000010,
	 0x03FF0000}, // ADDIU    T0, T0, 0x0010          ADDIU    __, __, 0x0010
	{0x03E00008,
	 0x00000000}, // JR       RA                      JR       RA
	{0x00000000, 0x00000000}, // NOP                              NOP
};

static const struct MaskPair OsInvalDCache[] = {
	{0x00000023,
	 0x03FFF800}, // SUBU     T0, T0, T2              SUBU     __, __, __
	{0xBC150000,
	 0x03E00000}, // CACHE    (D, CacheBarrier), T0, 0x0000 CACHE    (D, CacheBarrier), __, 0x0000
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T0, T1              SLTU     __, __, __
	{0x1000000E,
	 0x03E00000}, // BEQ      R0, 0x38(AT)            BEQ      R0, 0x38(__)
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x24000010,
	 0x03FF0000}, // ADDIU    T0, T0, 0x0010          ADDIU    __, __, 0x0010
	{0x3000000F,
	 0x03FF0000}, // ANDI     T2, T1, 0x000F          ANDI     __, __, 0x000F
	{0x10000006,
	 0x03E00000}, // BEQ      R0, 0x18(T2)            BEQ      R0, 0x18(__)
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x00000023,
	 0x03FFF800}, // SUBU     T1, T1, T2              SUBU     __, __, __
	{0xBC150010,
	 0x03E00000}, // CACHE    (D, CacheBarrier), T1, 0x0040 CACHE    (D, CacheBarrier), __, 0x0040
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T1, T0              SLTU     __, __, __
	{0x14000005,
	 0x03E00000}, // BNE      R0, 0x14(AT)            BNE      R0, 0x14(__)
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0xBC110000,
	 0x03E00000}, // CACHE    (D, IHitInvalidate), T0, 0x0000 CACHE    (D, IHitInvalidate), __, 0x0000
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T0, T1              SLTU     __, __, __
	{0x1400FFFD,
	 0x03E00000}, // BNE      R0, 0xFFFFFFF4(AT)      BNE      R0, 0xFFFFFFF4(__)
	{0x24000010,
	 0x03FF0000}, // ADDIU    T0, T0, 0x0010          ADDIU    __, __, 0x0010
	{0x03E00008,
	 0x00000000}, // JR       RA                      JR       RA
	{0x00000000, 0x00000000}, // NOP                              NOP
	{0x3C008000,
	 0x001F0000}, // LUI      T0, 0x8000              LUI      __, 0x8000
	{0x00000021,
	 0x03FFF800}, // ADDU     T1, T0, T3              ADDU     __, __, __
	{0x2400FFF0,
	 0x03FF0000}, // ADDIU    T1, T1, 0xFFF0          ADDIU    __, __, 0xFFF0
	{0xBC010000,
	 0x03E00000}, // CACHE    (D), T0, 0x0000         CACHE    (D), __, 0x0000
	{0x0000002B,
	 0x03FFF800}, // SLTU     AT, T0, T1              SLTU     __, __, __
	{0x1400FFFD,
	 0x03E00000}, // BNE      R0, 0xFFFFFFF4(AT)      BNE      R0, 0xFFFFFFF4(__)
	{0x24000010,
	 0x03FF0000}, // ADDIU    T0, T0, 0x0010          ADDIU    __, __, 0x0010
	{0x03E00008,
	 0x00000000}, // JR       RA                      JR       RA
	{0x00000000, 0x00000000}, // NOP                              NOP
};

static const struct MaskPair GPRSetup[] = {
	{0x3c1c0000, 0x0000ffff}, // LUI GP, 0x____
	{0x03E00008, 0x00000000}, // JR RA
	{0x279c0000, 0x0000ffff}, // ADDIU GP, GP, ____
};

static bool IsVAddr(uint32_t addr)
{
	if (0x80000000 != (0xff000000 & addr))
		return false;

	uint32_t off = addr & 0x00ffffff;
	if (off > 0x800000)
		return false;

	return true;
}

static std::vector<int> FindAllJumpsTo(const std::vector<uint32_t> &mem,
				       int pos)
{
	Instruction jmpInst;
	jmpInst.cmd = CMD_JAL;
	jmpInst.jump = static_cast<uint32_t>(4 * pos);
	uint32_t jmpInstVal = ToUInt(jmpInst);
	return FindAll(mem, jmpInstVal);
}

static std::set<int> FindAllJumpsTo(const std::vector<uint32_t> &mem,
				    const std::vector<int> &poses)
{
	std::set<int> jumps;
	for (int pos : poses) {
		std::vector<int> jumpResult = FindAllJumpsTo(mem, pos);
		jumps.insert(jumpResult.begin(), jumpResult.end());
	}
	return jumps;
}

static std::set<int> FindAllJumpsTo(const std::vector<uint32_t> &mem,
				    const uint32_t *data, size_t dataSize)
{
	std::vector<int> indices = IndicesOf(mem, data, dataSize);
	return FindAllJumpsTo(mem, indices);
}

static std::set<int> FindAllJumpsTo(const std::vector<uint32_t> &mem,
				    const MaskPair *data, size_t dataSize)
{
	std::vector<int> indices = IndicesOf(mem, data, dataSize);
	return FindAllJumpsTo(mem, indices);
}

static int CountJumps(const std::vector<uint32_t> &mem, int regionStart,
		      int regionEnd)
{
	std::set<uint32_t> jumps;
	for (int i = regionStart; i <= regionEnd; i++) {
		Instruction inst = decodeInstruction(mem[i]);
		if (inst.cmd == CMD_JAL) {
			jumps.emplace(*inst.jump);
		}
	}
	return static_cast<int>(jumps.size());
}

static uint32_t GetSecondArgumentToJAL(const std::vector<uint32_t> &mem,
				       uint32_t off)
{
	uint32_t vaddr = 0x80000000 | (off << 2);
	Interpreter interpreter(mem);
	const uint32_t InstructionsToInterpretCount = 16;
	const uint32_t BytesToInterpretCount = InstructionsToInterpretCount
					       << 2;
	interpreter.pc = vaddr - BytesToInterpretCount;

	for (uint32_t i = 0;
	     i < InstructionsToInterpretCount + 2 /*JAL + delay slot*/; i++) {
		std::optional<Instruction> inst = interpreter.getInstruction();
		if (inst.has_value())
			interpreter.execute(inst.value());
	}

	return static_cast<uint32_t>(interpreter.gpr[static_cast<int>(REG_A1)]);
}

static std::tuple<uint32_t, std::set<uint32_t>>
GetThirdArgumentToJALAndCheckWordStore(uint32_t gp,
				       const std::vector<uint32_t> &mem,
				       uint32_t off)
{
	uint32_t vaddr = 0x80000000 | (off << 2);
	Interpreter interpreter(mem);
	constexpr uint32_t InstructionsToInterpretCount = 20;
	constexpr uint32_t BytesToInterpretCount = InstructionsToInterpretCount
						   << 2;
	interpreter.pc = vaddr - BytesToInterpretCount;
	interpreter.gpr[static_cast<int>(REG_GP)] = gp;

	std::optional<Instruction> inst;

	std::set<uint32_t> wordsStored;
	for (uint32_t i = 0;
	     i < InstructionsToInterpretCount + 2 /*JAL + delay slot*/; i++) {
		inst = interpreter.getInstruction();
		if (inst.has_value()) {
			interpreter.execute(inst.value());
			if (inst.value().cmd == CMD_SW) {
				uint32_t wordStored =
					interpreter.gpr[static_cast<int>(
						*inst->rt)];
				if (wordStored != 0) {
					wordsStored.insert(wordStored);
				}
			}
		}
	}

	uint32_t a2 = interpreter.gpr[static_cast<int>(REG_A2)];
	return std::make_tuple(a2, wordsStored);
}

static bool IsPrologInstruction(const Instruction &inst)
{
	return inst.cmd == CMD_ADDIU && inst.rt == REG_SP &&
	       inst.rs == REG_SP && inst.imm < 0;
}

static int FindProlog(const std::vector<uint32_t> &mem, int start,
		      int maxScanLength)
{
	for (int i = start - 1; i > start - maxScanLength; i--) {
		Instruction inst = decodeInstruction(mem[i]);
		if (IsPrologInstruction(inst)) {
			return i;
		}
	}

	throw std::invalid_argument("Failed to detect the prolog");
}

template<typename T>
static std::set<T> GetViewBetween(const std::set<T> &s, const T &lower,
				  const T &upper)
{
	std::set<T> result;
	auto it_lower = s.lower_bound(lower);
	auto it_upper = s.upper_bound(upper);
	for (auto it = it_lower; it != it_upper; ++it) {
		result.insert(*it);
	}

	return result;
}

#define ARR_SZ(x) x, sizeof(x) / sizeof(*(x))

std::optional<AnalyzeResult> analyze(const std::vector<uint32_t> &mem)
{
	std::set<int> osGetCountJumps = FindAllJumpsTo(mem, ARR_SZ(OsGetCount));
	std::vector<int> disableOff;
	for (int off : IndicesOf(mem, ARR_SZ(OsDisableInt))) {
		disableOff.push_back(off);
		// sometimes there is a bit of a prologue before
		// TODO: Verify there are no tiny functions in [off-4, off] area
		disableOff.push_back(off - 4);
	}

	std::set<int> osDisableIntJumps = FindAllJumpsTo(mem, disableOff);
	std::set<int> osRestoreIntJumps =
		FindAllJumpsTo(mem, ARR_SZ(OsRestoreInt));

	// Discover all osGetTime functions that look like calls to 3 functions
	std::vector<int> osGetTimes;
	for (int regionStart : osDisableIntJumps) {
		try {
			const int MaxRegionLength = 0x18;
			auto view =
				GetViewBetween(osRestoreIntJumps, regionStart,
					       regionStart + MaxRegionLength);
			if (view.empty())
				continue;

			int regionEnd = *view.begin();
			view = GetViewBetween(osGetCountJumps, regionStart,
					      regionEnd);
			if (view.empty())
				continue;

			// Must be only calls to __osDisableInt + osGetCount + __osRestoreInt
			if (3 != CountJumps(mem, regionStart, regionEnd))
				continue;

			osGetTimes.push_back(
				FindProlog(mem, regionStart, 0x10));
		} catch (...) {
		}
	}

	std::vector<int> writebackDCacheOff;
	for (int off : IndicesOf(mem, ARR_SZ(OsWritebackDCache))) {
		writebackDCacheOff.push_back(off - 0xd);
	}
	std::set<int> osWritebackDCacheJumps =
		FindAllJumpsTo(mem, writebackDCacheOff);

	std::vector<int> invalOff;
	for (int off : IndicesOf(mem, ARR_SZ(OsInvalDCache))) {
		// sometimes there is an extra NOP inserted
		invalOff.push_back(off - 0xe);
		invalOff.push_back(off - 0xf);
	}
	std::set<int> osInvalDCacheJumps = FindAllJumpsTo(mem, invalOff);

	// Discover all __osSiRawStartDma that looks like calls to 3 functions with the 4th being after the prolog
	std::vector<int> osSiRawStartDmas;
	for (int regionStart : osWritebackDCacheJumps) {
		try {
			const int MaxRegionLength = 0x18;
			auto view =
				GetViewBetween(osInvalDCacheJumps, regionStart,
					       regionStart + MaxRegionLength);
			if (view.empty())
				continue;

			int regionEnd = *view.begin();

			// Must be only calls to osWritebackDCache + osVirtualToPhysical + osInvalDCache
			if (3 != CountJumps(mem, regionStart, regionEnd))
				continue;

			int prologAt = FindProlog(mem, regionStart, 0x20);
			for (int i = 0; i < 5; i++)
				osSiRawStartDmas.push_back(prologAt - i);
		} catch (...) {
		}
	}

	std::set<int> osGetTimeJumps = FindAllJumpsTo(mem, osGetTimes);
	std::set<int> osSiRawStartDmaJumps =
		FindAllJumpsTo(mem, osSiRawStartDmas);

	// Discover all osContInit; we do not need the functions themselves but __osContPifRam passed to __osSiRawStartDma
	// We know that 'osContInit' calls 'osGetTime' and '__osSiRawStartDma' 2 times
	std::vector<int> osContInts;
	for (int regionStart : osGetTimeJumps) {
		try {
			const int MaxRegionLength = 0x80;
			auto view = GetViewBetween(
				osSiRawStartDmaJumps, regionStart,
				regionStart + MaxRegionLength);
			if (view.size() != 2)
				continue;

			// Interpret the code around both JALs
			std::vector<uint32_t> osContPifRams;
			for (auto jump : view) {
				osContPifRams.push_back(GetSecondArgumentToJAL(
					mem, static_cast<uint32_t>(jump)));
			}

			if (osContPifRams[0] != osContPifRams[1])
				continue;

			uint32_t vosContPifRam = osContPifRams[0];
			if (!IsVAddr(vosContPifRam))
				continue;

			int prologAt = FindProlog(mem, regionStart, 0x20);
			for (int i = 0; i < 5; i++)
				osContInts.push_back(prologAt - i);
		} catch (...) {
		}
	}

	std::vector<int> gprSetups = IndicesOf(mem, ARR_SZ(GPRSetup));
	uint32_t gp = 0;
	if (!gprSetups.empty()) {
		uint32_t gprOff = static_cast<uint32_t>(gprSetups[0]);
		uint32_t gpHi = mem[gprOff] & 0xffff;
		int16_t gpLo = static_cast<int16_t>(mem[gprOff + 2] & 0xffff);
		gp = (gpHi << 16) + static_cast<uint32_t>(gpLo);
	}

	std::set<int> osContIntJumps = FindAllJumpsTo(mem, osContInts);
	for (int osContIntJump : osContIntJumps) {
		try {
			auto [status, wordStores] =
				GetThirdArgumentToJALAndCheckWordStore(
					gp, mem,
					static_cast<uint32_t>(osContIntJump));
			if (wordStores.size() < 2)
				continue;

			if (wordStores.find(status) == wordStores.end())
				continue;

			wordStores.erase(status);
			uint32_t cont = 0;
			for (const auto &stored : wordStores) {
				if (cont != 0) {
					long long dist0 = std::abs(
						static_cast<long long>(status) -
						static_cast<long long>(stored));
					long long dist1 = std::abs(
						static_cast<long long>(status) -
						static_cast<long long>(cont));
					if (dist0 < dist1)
						cont = stored;
				} else {
					cont = stored;
				}
			}

			int regionEnd = osContIntJump;
			int regionLength = 20;
			int regionStart = regionEnd - regionLength;
			std::vector<uint32_t> interpretedSegment(
				mem.begin() + regionStart,
				mem.begin() + regionStart + regionLength);

			return AnalyzeResult{regionStart,
					     std::move(interpretedSegment),
					     static_cast<int>(cont)};
		} catch (...) {
		}
	}

	return std::nullopt;
}
}
