//===-- llvm-superopt.cpp - LLVM Superoptimizer Tool ----------------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This utility enumerates and evaluates instruction sequences for
// superoptimization. Currently supports MOS targets only.
//
// Usage:
//   llvm-superopt --triple=mos --config=register  # Enumerate register ops
//   llvm-superopt --triple=mos --config=arithmetic # For ADC/SBC synthesis
//   llvm-superopt --triple=mos --find-optimal=mul3 # Find optimal mul-by-3
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/MOS/Superopt/Superopt.h"
#include "llvm/Target/MOS/Superopt/Verifier.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Emulator/Memory.h"
#include "llvm/Emulator/System.h"

// Forward declare MOS disassembler initialization
extern "C" void LLVMInitializeMOSDisassembler();

using namespace llvm;
using namespace llvm::superopt;

// Alias to resolve ambiguity with llvm::superopt::MOS
namespace MOSSuperopt = llvm::superopt::MOS;

static cl::OptionCategory SuperoptCategory("Superoptimizer Options");

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple (e.g., mos)"),
               cl::value_desc("triple"), cl::init("mos"),
               cl::cat(SuperoptCategory));

static cl::opt<std::string>
    MCPU("mcpu", cl::desc("Target CPU"), cl::value_desc("cpu-name"),
         cl::init(""), cl::cat(SuperoptCategory));

enum ConfigType {
  Config_Register,
  Config_Arithmetic,
  Config_Shift,
  Config_Custom,
};

static cl::opt<ConfigType> ConfigOpt(
    "config", cl::desc("Enumeration configuration:"), cl::init(Config_Register),
    cl::values(clEnumValN(Config_Register, "register",
                          "Register operations only (default)"),
               clEnumValN(Config_Arithmetic, "arithmetic",
                          "Arithmetic synthesis (ADC/SBC/AND/OR/EOR)"),
               clEnumValN(Config_Shift, "shift",
                          "Shift and rotate operations"),
               clEnumValN(Config_Custom, "custom", "Custom configuration")),
    cl::cat(SuperoptCategory));

static cl::opt<std::string> FindOptimal(
    "find-optimal",
    cl::desc("Find optimal sequence for operation (e.g., mul3, neg, shl2)"),
    cl::value_desc("operation"), cl::init(""), cl::cat(SuperoptCategory));

static cl::opt<unsigned>
    MaxLength("max-length", cl::desc("Maximum sequence length (default 3)"),
              cl::init(3), cl::cat(SuperoptCategory));

static cl::opt<unsigned>
    MaxBytes("max-bytes", cl::desc("Maximum total bytes (default 8)"),
             cl::init(8), cl::cat(SuperoptCategory));

static cl::opt<unsigned>
    MaxCycles("max-cycles", cl::desc("Maximum total cycles (default 16)"),
              cl::init(16), cl::cat(SuperoptCategory));

static cl::opt<uint64_t>
    MaxCandidates("max-candidates",
                  cl::desc("Maximum candidates to emit (0=unlimited)"),
                  cl::init(100), cl::cat(SuperoptCategory));

static cl::opt<bool>
    Verbose("verbose", cl::desc("Show enumeration statistics"), cl::init(false),
            cl::cat(SuperoptCategory));

static cl::opt<bool>
    NoPrune("no-prune", cl::desc("Disable pruning heuristics"), cl::init(false),
            cl::cat(SuperoptCategory));

static cl::opt<bool> ListOpcodes("list-opcodes",
                                 cl::desc("List available opcodes and exit"),
                                 cl::init(false), cl::cat(SuperoptCategory));

static cl::opt<bool> ListOperations("list-operations",
                                    cl::desc("List available operations for --find-optimal"),
                                    cl::init(false), cl::cat(SuperoptCategory));

/// Parse the --find-optimal argument and return an OperationSpec.
static std::optional<OperationSpec> parseOperation(StringRef OpName) {
  // Multiply by constant: mul2, mul3, mul5, etc.
  if (OpName.consume_front("mul")) {
    unsigned K;
    if (OpName.getAsInteger(10, K) || K == 0)
      return std::nullopt;
    return ops::mulByConst(K);
  }

  // Left shift by constant: shl1, shl2, etc.
  if (OpName.consume_front("shl")) {
    unsigned N;
    if (OpName.getAsInteger(10, N) || N > 7)
      return std::nullopt;
    return ops::shlByConst(N);
  }

  // Logical right shift: lshr1, lshr2, etc.
  if (OpName.consume_front("lshr")) {
    unsigned N;
    if (OpName.getAsInteger(10, N) || N > 7)
      return std::nullopt;
    return ops::lshrByConst(N);
  }

  // Arithmetic right shift: ashr1, ashr2, etc.
  if (OpName.consume_front("ashr")) {
    unsigned N;
    if (OpName.getAsInteger(10, N) || N > 7)
      return std::nullopt;
    return ops::ashrByConst(N);
  }

  // Negate
  if (OpName == "neg")
    return ops::neg();

  // Bitwise NOT
  if (OpName == "not")
    return ops::notA();

  // A + X
  if (OpName == "add")
    return ops::addAX();

  // A - X
  if (OpName == "sub")
    return ops::subAX();

  // A * X
  if (OpName == "mul")
    return ops::mulAX();

  // A & X
  if (OpName == "and")
    return ops::andAX();

  // A | X
  if (OpName == "or")
    return ops::orAX();

  // A ^ X
  if (OpName == "xor")
    return ops::xorAX();

  // Swap A and X
  if (OpName == "swap")
    return ops::swapAX();

  //===--------------------------------------------------------------------===//
  // Conditional Operations
  //===--------------------------------------------------------------------===//

  // max(A, X) - unsigned
  if (OpName == "max")
    return ops::maxAX();

  // min(A, X) - unsigned
  if (OpName == "min")
    return ops::minAX();

  // smax(A, X) - signed
  if (OpName == "smax")
    return ops::smaxAX();

  // smin(A, X) - signed
  if (OpName == "smin")
    return ops::sminAX();

  // abs(A) - signed absolute value
  if (OpName == "abs")
    return ops::absA();

  // clamp(A, 0, X)
  if (OpName == "clamp")
    return ops::clampAX();

  //===--------------------------------------------------------------------===//
  // Bit Manipulation
  //===--------------------------------------------------------------------===//

  // Reverse bits
  if (OpName == "bitrev")
    return ops::bitrev();

  // Sign extend from 4 bits
  if (OpName == "sext4")
    return ops::sext4();

  // Sign extend from N bits: sext1, sext2, ..., sext7
  if (OpName.consume_front("sext")) {
    unsigned N;
    if (OpName.getAsInteger(10, N) || N < 1 || N > 7)
      return std::nullopt;
    return ops::sextFromBit(N);
  }

  // Count leading zeros
  if (OpName == "clz")
    return ops::clz();

  // Count trailing zeros
  if (OpName == "ctz")
    return ops::ctz();

  // Population count
  if (OpName == "popcount")
    return ops::popcount();

  // Isolate lowest set bit
  if (OpName == "isolate")
    return ops::isolateLowBit();

  // Clear lowest set bit
  if (OpName == "clearlow")
    return ops::clearLowBit();

  //===--------------------------------------------------------------------===//
  // Arithmetic Variations
  //===--------------------------------------------------------------------===//

  // Average (floor)
  if (OpName == "avg")
    return ops::avgAX();

  // Average (ceil)
  if (OpName == "avgceil")
    return ops::avgCeilAX();

  // Saturating add
  if (OpName == "sadd")
    return ops::saddAX();

  // Saturating subtract
  if (OpName == "ssub")
    return ops::ssubAX();

  // Multiply high byte
  if (OpName == "mulhi")
    return ops::mulhiAX();

  return std::nullopt;
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  // Initialize all targets and MOS disassembler
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  LLVMInitializeMOSDisassembler();

  cl::HideUnrelatedOptions(SuperoptCategory);
  cl::ParseCommandLineOptions(argc, argv, "LLVM Superoptimizer\n\n"
                                          "Enumerates instruction sequences for superoptimization.\n"
                                          "Currently supports MOS (6502) targets.\n");

  // List operations mode
  if (ListOperations) {
    outs() << "Available operations for --find-optimal:\n";
    outs() << "\n  Arithmetic (A -> A):\n";
    outs() << "    mulN   - Multiply A by N (e.g., mul3, mul5, mul10)\n";
    outs() << "    shlN   - Left shift A by N bits (e.g., shl1, shl2)\n";
    outs() << "    lshrN  - Logical right shift A by N bits\n";
    outs() << "    ashrN  - Arithmetic right shift A by N bits\n";
    outs() << "    neg    - Negate A (two's complement)\n";
    outs() << "    not    - Bitwise NOT A\n";
    outs() << "    abs    - Absolute value (signed)\n";
    outs() << "\n  Arithmetic (A, X -> A):\n";
    outs() << "    add    - A + X\n";
    outs() << "    sub    - A - X\n";
    outs() << "    mul    - A * X (low byte)\n";
    outs() << "    mulhi  - A * X >> 8 (high byte)\n";
    outs() << "    and    - A & X\n";
    outs() << "    or     - A | X\n";
    outs() << "    xor    - A ^ X\n";
    outs() << "    avg    - (A + X) / 2 (floor)\n";
    outs() << "    avgceil- (A + X + 1) / 2 (ceil)\n";
    outs() << "    sadd   - Saturating add (clamp to 255)\n";
    outs() << "    ssub   - Saturating subtract (clamp to 0)\n";
    outs() << "\n  Conditional (A, X -> A):\n";
    outs() << "    max    - max(A, X) unsigned\n";
    outs() << "    min    - min(A, X) unsigned\n";
    outs() << "    smax   - max(A, X) signed\n";
    outs() << "    smin   - min(A, X) signed\n";
    outs() << "    clamp  - clamp A to [0, X]\n";
    outs() << "\n  Bit Manipulation (A -> A):\n";
    outs() << "    bitrev - Reverse bits in A\n";
    outs() << "    sextN  - Sign extend from N bits (e.g., sext4, sext7)\n";
    outs() << "    clz    - Count leading zeros\n";
    outs() << "    ctz    - Count trailing zeros\n";
    outs() << "    popcount - Population count (count set bits)\n";
    outs() << "    isolate  - Isolate lowest set bit (A & -A)\n";
    outs() << "    clearlow - Clear lowest set bit (A & (A-1))\n";
    outs() << "\n  Register (A, X -> A, X):\n";
    outs() << "    swap   - Exchange A and X\n";
    return 0;
  }

  // Look up the target
  std::string Error;
  Triple TheTriple(Triple::normalize(TripleName));
  const Target *TheTarget =
      TargetRegistry::lookupTarget(TheTriple, Error);

  if (!TheTarget) {
    WithColor::error(errs()) << "unable to get target for '" << TripleName
                             << "': " << Error << "\n";
    return 1;
  }

  // Check if this is MOS - we only support MOS currently
  if (TheTriple.getArch() != Triple::mos) {
    WithColor::error(errs())
        << "llvm-superopt currently only supports MOS targets\n";
    return 1;
  }

  // Create MC components
  std::unique_ptr<MCRegisterInfo> MRI(TheTarget->createMCRegInfo(TheTriple));
  if (!MRI) {
    WithColor::error(errs()) << "unable to create register info\n";
    return 1;
  }

  MCTargetOptions MCOptions;
  std::unique_ptr<MCAsmInfo> MAI(
      TheTarget->createMCAsmInfo(*MRI, TheTriple, MCOptions));
  if (!MAI) {
    WithColor::error(errs()) << "unable to create asm info\n";
    return 1;
  }

  std::unique_ptr<MCInstrInfo> MII(TheTarget->createMCInstrInfo());
  if (!MII) {
    WithColor::error(errs()) << "unable to create instruction info\n";
    return 1;
  }

  std::unique_ptr<MCSubtargetInfo> STI(
      TheTarget->createMCSubtargetInfo(TheTriple, MCPU, ""));
  if (!STI) {
    WithColor::error(errs()) << "unable to create subtarget info\n";
    return 1;
  }

  // List opcodes mode
  if (ListOpcodes) {
    outs() << "Available opcodes for " << TripleName << ":\n";
    for (unsigned I = 0; I < MII->getNumOpcodes(); ++I) {
      StringRef Name = MII->getName(I);
      if (!Name.empty() && !Name.starts_with("PSEUDO") &&
          !Name.starts_with("G_") && !Name.starts_with("COPY") &&
          !Name.starts_with("PHI") && !Name.starts_with("IMPLICIT") &&
          !Name.starts_with("SUBREG") && !Name.starts_with("DBG") &&
          !Name.starts_with("REG_SEQUENCE") && !Name.starts_with("KILL") &&
          !Name.starts_with("BUNDLE") && !Name.starts_with("INLINEASM") &&
          !Name.starts_with("LIFETIME") && !Name.starts_with("ANNOTATION") &&
          !Name.starts_with("GC_LABEL") && !Name.starts_with("CFI")) {
        const MCInstrDesc &Desc = MII->get(I);
        outs() << "  " << I << ": " << Name << " (size=" << Desc.getSize()
               << ", operands=" << Desc.getNumOperands() << ")\n";
      }
    }
    return 0;
  }

  // Create instruction printer for output
  std::unique_ptr<MCInstPrinter> IP(TheTarget->createMCInstPrinter(
      TheTriple, /*SyntaxVariant=*/0, *MAI, *MII, *MRI));
  if (!IP) {
    WithColor::error(errs()) << "unable to create instruction printer\n";
    return 1;
  }

  // Find optimal mode - use verification
  if (!FindOptimal.empty()) {
    auto Spec = parseOperation(FindOptimal);
    if (!Spec) {
      WithColor::error(errs()) << "unknown operation '" << FindOptimal
                               << "'. Use --list-operations to see available operations.\n";
      return 1;
    }

    outs() << "Finding optimal sequence for: " << Spec->Name << "\n";

    // Create MCContext - needed for emulator creation via TargetRegistry
    MCContext MCCtx(TheTriple, MAI.get(), MRI.get(), STI.get());

    // Create emulator context via TargetRegistry (proper LLVM pattern)
    std::unique_ptr<emu::Context> Ctx(TheTarget->createEmulator(*STI, MCCtx));
    if (!Ctx) {
      WithColor::error(errs()) << "unable to create MOS execution context\n";
      return 1;
    }

    // Set up a System with 64KB RAM so memory operations work
    emu::System Sys;
    emu::Memory RAM(65536); // 64KB RAM
    Sys.addDevice(0, 0xFFFF, &RAM);
    Sys.addContext(Ctx.get());

    // Find optimal sequence
    auto BestSeq = findOptimal(*Spec, *Ctx, *MII, *STI, MaxLength, Verbose);

    if (BestSeq.empty()) {
      outs() << "No valid sequence found within constraints.\n";
    } else {
      outs() << "\nOptimal sequence:\n";
      for (const MCInst &Inst : BestSeq) {
        outs() << "  ";
        IP->printInst(&Inst, /*Address=*/0, /*Annot=*/"", *STI, outs());
        outs() << "\n";
      }
    }

    return 0;
  }

  // Create MOS cost model
  std::unique_ptr<CostModel> CM = MOSSuperopt::createCostModel(*MII, *STI);

  // Get appropriate config
  EnumeratorConfig Config;
  switch (ConfigOpt) {
  case Config_Register:
    Config = MOSSuperopt::getRegisterOnlyConfig(*MII);
    break;
  case Config_Arithmetic:
    Config = MOSSuperopt::getArithmeticConfig(*MII);
    break;
  case Config_Shift:
    Config = MOSSuperopt::getShiftConfig(*MII);
    break;
  case Config_Custom:
    // For custom, user specifies constraints - use minimal config
    Config = MOSSuperopt::getRegisterOnlyConfig(*MII);
    break;
  }

  // Apply command line overrides
  Config.MaxLength = MaxLength;
  Config.MaxCost = Cost(MaxBytes, MaxCycles);
  Config.EnablePruning = !NoPrune;
  Config.Verbose = Verbose;

  // Validate config
  if (Config.Templates.empty()) {
    WithColor::error(errs())
        << "no instruction templates configured (target may not support "
           "required opcodes)\n";
    return 1;
  }

  if (Verbose) {
    outs() << "Configuration:\n";
    outs() << "  Max length: " << Config.MaxLength << "\n";
    outs() << "  Max cost: " << Config.MaxCost.Bytes << " bytes, "
           << Config.MaxCost.Cycles << " cycles\n";
    outs() << "  Templates: " << Config.Templates.size() << "\n";
    outs() << "  Pruning: " << (Config.EnablePruning ? "enabled" : "disabled")
           << "\n";
    outs() << "\n";
  }

  // Create enumerator
  Enumerator Enum(*MII, *STI, *CM, Config);

  // Run enumeration
  uint64_t Count = 0;
  Enum.run([&](ArrayRef<MCInst> Seq, const Cost &SeqCost) -> bool {
    // Print sequence
    outs() << "[" << SeqCost.Bytes << "B, " << SeqCost.Cycles << "C] ";
    for (size_t I = 0; I < Seq.size(); ++I) {
      if (I > 0)
        outs() << "; ";
      IP->printInst(&Seq[I], /*Address=*/0, /*Annot=*/"", *STI, outs());
    }
    outs() << "\n";

    ++Count;
    if (MaxCandidates > 0 && Count >= MaxCandidates) {
      outs() << "... (stopped at " << MaxCandidates << " candidates)\n";
      return false;
    }
    return true;
  });

  // Print stats
  const EnumeratorStats &Stats = Enum.getStats();
  if (Verbose) {
    outs() << "\nStatistics:\n";
    outs() << "  Total states: " << Stats.TotalGenerated << "\n";
    outs() << "  Pruned: " << Stats.Pruned << "\n";
    outs() << "  Cost pruned: " << Stats.CostPruned << "\n";
    outs() << "  Emitted: " << Stats.Emitted << "\n";
  } else {
    outs() << "\nEmitted " << Count << " candidates\n";
  }

  return 0;
}
