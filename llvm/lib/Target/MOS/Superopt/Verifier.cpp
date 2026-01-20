//===-- Verifier.cpp - Superoptimizer verification ------------------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/MOS/Superopt/Verifier.h"
#include "../MCTargetDesc/MOSContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/MOS/Superopt/Superopt.h"

using namespace llvm;
using namespace llvm::superopt;

// Use fully-qualified name to distinguish from superopt::MOS
Verifier::Verifier(llvm::MOS::Context &Ctx, const MCInstrInfo &MII)
    : Ctx(Ctx), MII(MII) {}

void Verifier::resetState() {
  // Reset all registers to known state
  // We need to access these through the Context interface
  // For now, use the reset() method if available
  Ctx.reset();
}

void Verifier::executeSequence(ArrayRef<MCInst> Seq) {
  for (const MCInst &Inst : Seq) {
    // Execute using SAIL-generated semantics via the public interface
    Ctx.executeInst(Inst);
  }
}

bool Verifier::testSingleInput(ArrayRef<MCInst> Seq, const OperationSpec &Spec,
                                uint8_t InputA, uint8_t InputX,
                                VerifyResult &Result) {
  // Reset to known state
  Ctx.A = 0;
  Ctx.X = 0;
  Ctx.Y = 0;
  Ctx.S = 0xFF;
  Ctx.C = false;
  Ctx.Z = false;
  Ctx.I = false;
  Ctx.D = false;
  Ctx.V = false;
  Ctx.N = false;

  // Set input registers based on spec
  if (Spec.IO.UsesA)
    Ctx.A = InputA;
  if (Spec.IO.UsesX)
    Ctx.X = InputX;
  if (Spec.IO.UsesCarry)
    Ctx.C = false;  // Default carry state

  // Execute the sequence
  executeSequence(Seq);

  // Compute expected output and check
  if (Spec.ComputeAXtoAX) {
    // (A, X) -> (A, X) operation (e.g., swap)
    auto [ExpectedA, ExpectedX] = Spec.ComputeAXtoAX(InputA, InputX);
    if (Ctx.A != ExpectedA) {
      Result.Success = false;
      Result.FailingInputA = InputA;
      Result.FailingInputX = InputX;
      Result.ExpectedOutput = ExpectedA;
      Result.ActualOutput = Ctx.A;
      Result.ErrorMessage = "A register mismatch";
      return false;
    }
    if (Ctx.X != ExpectedX) {
      Result.Success = false;
      Result.FailingInputA = InputA;
      Result.FailingInputX = InputX;
      Result.ExpectedOutput = ExpectedX;
      Result.ActualOutput = Ctx.X;
      Result.ErrorMessage = "X register mismatch";
      return false;
    }
    return true;
  }

  uint8_t Expected = 0;
  if (Spec.ComputeA) {
    Expected = Spec.ComputeA(InputA);
  } else if (Spec.ComputeAX) {
    Expected = Spec.ComputeAX(InputA, InputX);
  }

  // Check output
  if (Spec.IO.DefsA && Ctx.A != Expected) {
    Result.Success = false;
    Result.FailingInputA = InputA;
    if (Spec.IO.UsesX)
      Result.FailingInputX = InputX;
    Result.ExpectedOutput = Expected;
    Result.ActualOutput = Ctx.A;
    return false;
  }

  return true;
}

VerifyResult Verifier::verify(ArrayRef<MCInst> Seq, const OperationSpec &Spec) {
  VerifyResult Result;
  Result.Success = true;
  LastTestCount = 0;

  // For A -> A operations: test all 256 inputs
  if (Spec.ComputeA) {
    for (unsigned A = 0; A < 256; ++A) {
      ++LastTestCount;
      if (!testSingleInput(Seq, Spec, A, 0, Result)) {
        return Result;
      }
    }
    return Result;
  }

  // For (A, X) -> A operations: test all 256*256 = 65536 inputs
  if (Spec.ComputeAX) {
    for (unsigned A = 0; A < 256; ++A) {
      for (unsigned X = 0; X < 256; ++X) {
        ++LastTestCount;
        if (!testSingleInput(Seq, Spec, A, X, Result)) {
          return Result;
        }
      }
    }
    return Result;
  }

  // For (A, X) -> (A, X) operations: test all 256*256 = 65536 inputs
  if (Spec.ComputeAXtoAX) {
    for (unsigned A = 0; A < 256; ++A) {
      for (unsigned X = 0; X < 256; ++X) {
        ++LastTestCount;
        if (!testSingleInput(Seq, Spec, A, X, Result)) {
          return Result;
        }
      }
    }
    return Result;
  }

  Result.Success = false;
  Result.ErrorMessage = "No computation function defined for spec";
  return Result;
}

//===----------------------------------------------------------------------===//
// findOptimal - Main superoptimization entry point
//===----------------------------------------------------------------------===//

std::vector<MCInst> superopt::findOptimal(const OperationSpec &Spec,
                                           llvm::emu::Context &Ctx,
                                           const MCInstrInfo &MII,
                                           const MCSubtargetInfo &STI,
                                           unsigned MaxLength,
                                           bool Verbose) {
  // Cast to MOS::Context - the caller guarantees this is valid
  auto &MOSCtx = static_cast<llvm::MOS::Context &>(Ctx);

  // Create verifier
  Verifier V(MOSCtx, MII);

  // Create cost model
  auto CM = superopt::MOS::createCostModel(MII, STI);

  // Get appropriate config based on operation
  EnumeratorConfig Config;
  if (Spec.IO.UsesA && !Spec.IO.UsesX) {
    // Unary operation on A - use register + immediate config
    Config = superopt::MOS::getArithmeticConfig(MII);
  } else {
    // Binary operation - include register transfers
    Config = superopt::MOS::getRegisterOnlyConfig(MII);
  }
  Config.MaxLength = MaxLength;
  Config.Verbose = Verbose;

  // Track best solution
  std::vector<MCInst> BestSeq;
  Cost BestCost = {UINT_MAX, UINT_MAX};

  // Statistics
  uint64_t Verified = 0;
  uint64_t Failed = 0;

  // Create enumerator and run
  Enumerator E(MII, STI, *CM, Config);
  E.run([&](ArrayRef<MCInst> Seq, const Cost &SeqCost) -> bool {
    // Skip if worse than current best
    if (!(SeqCost < BestCost))
      return true;

    // Verify this sequence
    VerifyResult VR = V.verify(Seq, Spec);
    if (VR.Success) {
      ++Verified;
      BestSeq.assign(Seq.begin(), Seq.end());
      BestCost = SeqCost;

      if (Verbose) {
        errs() << "Found valid sequence (cost " << SeqCost.Bytes << "b/"
               << SeqCost.Cycles << "c):\n";
        for (const MCInst &Inst : Seq) {
          errs() << "  " << MII.getName(Inst.getOpcode()) << "\n";
        }
      }
    } else {
      ++Failed;
    }

    return true;  // Continue searching
  });

  if (Verbose) {
    errs() << "Search complete: " << Verified << " valid, " << Failed
           << " failed\n";
    if (!BestSeq.empty()) {
      errs() << "Best sequence (" << BestCost.Bytes << " bytes, "
             << BestCost.Cycles << " cycles):\n";
      for (const MCInst &Inst : BestSeq) {
        errs() << "  " << MII.getName(Inst.getOpcode()) << "\n";
      }
    }
  }

  return BestSeq;
}
