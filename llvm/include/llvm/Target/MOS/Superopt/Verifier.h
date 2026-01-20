//===-- Verifier.h - Superoptimizer verification ----------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the verification interface for the superoptimizer.
// Verification checks that a candidate instruction sequence correctly
// implements a specified operation.
//
// For 8-bit operations, we use exhaustive testing (256 inputs).
// For larger operations, symbolic verification could be added.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_MOS_SUPEROPT_VERIFIER_H
#define LLVM_TARGET_MOS_SUPEROPT_VERIFIER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/MC/MCInst.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace llvm {

class MCInstrInfo;
class MCSubtargetInfo;

namespace emu {
class Context;
} // namespace emu

namespace MOS {
class Context;
}

namespace superopt {

//===----------------------------------------------------------------------===//
// Operation Specifications
//===----------------------------------------------------------------------===//

/// Describes what registers/flags are inputs and outputs for an operation.
struct OperationIO {
  // Input registers (read before operation)
  bool UsesA = false;
  bool UsesX = false;
  bool UsesY = false;
  bool UsesCarry = false;

  // Output registers (written by operation)
  bool DefsA = false;
  bool DefsX = false;
  bool DefsY = false;
  bool DefsCarry = false;
  bool DefsZero = false;
  bool DefsNegative = false;
  bool DefsOverflow = false;
};

/// Specification for an operation to superoptimize.
/// The spec function takes input values and returns the expected output.
struct OperationSpec {
  std::string Name;
  OperationIO IO;

  /// For i8 -> i8 operations (e.g., multiply by constant).
  /// Input: A register value. Output: expected A register value.
  std::function<uint8_t(uint8_t)> ComputeA;

  /// For i8 x i8 -> i8 operations (e.g., general multiply).
  /// Input: A and X values. Output: expected A value.
  std::function<uint8_t(uint8_t, uint8_t)> ComputeAX;

  /// For operations that also produce a carry (e.g., add with carry out).
  /// Returns {result, carry}.
  std::function<std::pair<uint8_t, bool>(uint8_t, uint8_t, bool)> ComputeWithCarry;

  /// For (A, X) -> (A, X) operations (e.g., swap).
  /// Input: A and X values. Output: {expected A, expected X}.
  std::function<std::pair<uint8_t, uint8_t>(uint8_t, uint8_t)> ComputeAXtoAX;

  /// Create an A -> A operation spec.
  static OperationSpec unaryA(const std::string &Name,
                              std::function<uint8_t(uint8_t)> Fn) {
    OperationSpec Spec;
    Spec.Name = Name;
    Spec.IO.UsesA = true;
    Spec.IO.DefsA = true;
    Spec.IO.DefsZero = true;
    Spec.IO.DefsNegative = true;
    Spec.ComputeA = std::move(Fn);
    return Spec;
  }

  /// Create an (A, X) -> A operation spec.
  static OperationSpec binaryAX(const std::string &Name,
                                std::function<uint8_t(uint8_t, uint8_t)> Fn) {
    OperationSpec Spec;
    Spec.Name = Name;
    Spec.IO.UsesA = true;
    Spec.IO.UsesX = true;
    Spec.IO.DefsA = true;
    Spec.IO.DefsZero = true;
    Spec.IO.DefsNegative = true;
    Spec.ComputeAX = std::move(Fn);
    return Spec;
  }

  /// Create an (A, X) -> (A, X) operation spec (e.g., swap).
  static OperationSpec swapAXSpec(const std::string &Name,
                                  std::function<std::pair<uint8_t, uint8_t>(uint8_t, uint8_t)> Fn) {
    OperationSpec Spec;
    Spec.Name = Name;
    Spec.IO.UsesA = true;
    Spec.IO.UsesX = true;
    Spec.IO.DefsA = true;
    Spec.IO.DefsX = true;
    Spec.ComputeAXtoAX = std::move(Fn);
    return Spec;
  }
};

//===----------------------------------------------------------------------===//
// Built-in Operation Specs
//===----------------------------------------------------------------------===//

namespace ops {

/// Multiply A by a constant.
inline OperationSpec mulByConst(uint8_t K) {
  return OperationSpec::unaryA("mul_" + std::to_string(K),
                               [K](uint8_t A) { return A * K; });
}

/// Left shift A by N bits.
inline OperationSpec shlByConst(unsigned N) {
  return OperationSpec::unaryA("shl_" + std::to_string(N),
                               [N](uint8_t A) { return A << N; });
}

/// Logical right shift A by N bits.
inline OperationSpec lshrByConst(unsigned N) {
  return OperationSpec::unaryA("lshr_" + std::to_string(N),
                               [N](uint8_t A) { return A >> N; });
}

/// Arithmetic right shift A by N bits.
inline OperationSpec ashrByConst(unsigned N) {
  return OperationSpec::unaryA(
      "ashr_" + std::to_string(N),
      [N](uint8_t A) { return static_cast<uint8_t>(static_cast<int8_t>(A) >> N); });
}

/// Negate A (two's complement).
inline OperationSpec neg() {
  return OperationSpec::unaryA("neg", [](uint8_t A) { return -A; });
}

/// Bitwise NOT A.
inline OperationSpec notA() {
  return OperationSpec::unaryA("not", [](uint8_t A) { return ~A; });
}

/// A + X
inline OperationSpec addAX() {
  return OperationSpec::binaryAX("add_ax",
                                 [](uint8_t A, uint8_t X) { return A + X; });
}

/// A - X
inline OperationSpec subAX() {
  return OperationSpec::binaryAX("sub_ax",
                                 [](uint8_t A, uint8_t X) { return A - X; });
}

/// A * X
inline OperationSpec mulAX() {
  return OperationSpec::binaryAX("mul_ax",
                                 [](uint8_t A, uint8_t X) { return A * X; });
}

/// A & X
inline OperationSpec andAX() {
  return OperationSpec::binaryAX("and_ax",
                                 [](uint8_t A, uint8_t X) { return A & X; });
}

/// A | X
inline OperationSpec orAX() {
  return OperationSpec::binaryAX("or_ax",
                                 [](uint8_t A, uint8_t X) { return A | X; });
}

/// A ^ X
inline OperationSpec xorAX() {
  return OperationSpec::binaryAX("xor_ax",
                                 [](uint8_t A, uint8_t X) { return A ^ X; });
}

/// Swap A and X registers.
inline OperationSpec swapAX() {
  return OperationSpec::swapAXSpec("swap_ax", [](uint8_t A, uint8_t X) {
    return std::make_pair(X, A);  // A gets X's value, X gets A's value
  });
}

//===----------------------------------------------------------------------===//
// Conditional Operations
//===----------------------------------------------------------------------===//

/// max(A, X) - unsigned maximum
inline OperationSpec maxAX() {
  return OperationSpec::binaryAX("max_ax",
                                 [](uint8_t A, uint8_t X) { return A > X ? A : X; });
}

/// min(A, X) - unsigned minimum
inline OperationSpec minAX() {
  return OperationSpec::binaryAX("min_ax",
                                 [](uint8_t A, uint8_t X) { return A < X ? A : X; });
}

/// smax(A, X) - signed maximum
inline OperationSpec smaxAX() {
  return OperationSpec::binaryAX("smax_ax", [](uint8_t A, uint8_t X) {
    int8_t sA = static_cast<int8_t>(A);
    int8_t sX = static_cast<int8_t>(X);
    return static_cast<uint8_t>(sA > sX ? sA : sX);
  });
}

/// smin(A, X) - signed minimum
inline OperationSpec sminAX() {
  return OperationSpec::binaryAX("smin_ax", [](uint8_t A, uint8_t X) {
    int8_t sA = static_cast<int8_t>(A);
    int8_t sX = static_cast<int8_t>(X);
    return static_cast<uint8_t>(sA < sX ? sA : sX);
  });
}

/// abs(A) - absolute value (signed)
inline OperationSpec absA() {
  return OperationSpec::unaryA("abs", [](uint8_t A) {
    int8_t sA = static_cast<int8_t>(A);
    return static_cast<uint8_t>(sA < 0 ? -sA : sA);
  });
}

/// clamp(A, 0, X) - clamp A to range [0, X] (unsigned)
inline OperationSpec clampAX() {
  return OperationSpec::binaryAX("clamp_ax",
                                 [](uint8_t A, uint8_t X) { return A > X ? X : A; });
}

//===----------------------------------------------------------------------===//
// Bit Manipulation
//===----------------------------------------------------------------------===//

/// Reverse bits in A
inline OperationSpec bitrev() {
  return OperationSpec::unaryA("bitrev", [](uint8_t A) {
    uint8_t result = 0;
    for (int i = 0; i < 8; ++i) {
      result |= ((A >> i) & 1) << (7 - i);
    }
    return result;
  });
}

/// Sign extend from 4 bits to 8 bits
inline OperationSpec sext4() {
  return OperationSpec::unaryA("sext4", [](uint8_t A) {
    uint8_t val = A & 0x0F;
    return (val & 0x08) ? (val | 0xF0) : val;
  });
}

/// Sign extend from bit N to 8 bits
inline OperationSpec sextFromBit(unsigned N) {
  return OperationSpec::unaryA("sext" + std::to_string(N), [N](uint8_t A) {
    uint8_t mask = (1 << N) - 1;
    uint8_t val = A & mask;
    uint8_t signBit = 1 << (N - 1);
    return (val & signBit) ? (val | ~mask) : val;
  });
}

/// Count leading zeros
inline OperationSpec clz() {
  return OperationSpec::unaryA("clz", [](uint8_t A) {
    if (A == 0) return uint8_t(8);
    uint8_t count = 0;
    while ((A & 0x80) == 0) {
      count++;
      A <<= 1;
    }
    return count;
  });
}

/// Count trailing zeros
inline OperationSpec ctz() {
  return OperationSpec::unaryA("ctz", [](uint8_t A) {
    if (A == 0) return uint8_t(8);
    uint8_t count = 0;
    while ((A & 1) == 0) {
      count++;
      A >>= 1;
    }
    return count;
  });
}

/// Population count (count set bits)
inline OperationSpec popcount() {
  return OperationSpec::unaryA("popcount", [](uint8_t A) {
    uint8_t count = 0;
    while (A) {
      count += A & 1;
      A >>= 1;
    }
    return count;
  });
}

/// Isolate lowest set bit: A & -A
inline OperationSpec isolateLowBit() {
  return OperationSpec::unaryA("isolate_low", [](uint8_t A) {
    return A & (-A);
  });
}

/// Clear lowest set bit: A & (A - 1)
inline OperationSpec clearLowBit() {
  return OperationSpec::unaryA("clear_low", [](uint8_t A) {
    return A & (A - 1);
  });
}

//===----------------------------------------------------------------------===//
// Arithmetic Variations
//===----------------------------------------------------------------------===//

/// Average of A and X (unsigned, floor): (A + X) / 2
inline OperationSpec avgAX() {
  return OperationSpec::binaryAX("avg_ax",
                                 [](uint8_t A, uint8_t X) { return (A + X) >> 1; });
}

/// Average of A and X (unsigned, ceil): (A + X + 1) / 2
inline OperationSpec avgCeilAX() {
  return OperationSpec::binaryAX("avgceil_ax",
                                 [](uint8_t A, uint8_t X) { return (A + X + 1) >> 1; });
}

/// Saturating add (clamp to 255)
inline OperationSpec saddAX() {
  return OperationSpec::binaryAX("sadd_ax", [](uint8_t A, uint8_t X) {
    unsigned sum = A + X;
    return sum > 255 ? uint8_t(255) : uint8_t(sum);
  });
}

/// Saturating subtract (clamp to 0)
inline OperationSpec ssubAX() {
  return OperationSpec::binaryAX("ssub_ax", [](uint8_t A, uint8_t X) {
    return A > X ? uint8_t(A - X) : uint8_t(0);
  });
}

/// Multiply high byte: (A * X) >> 8
inline OperationSpec mulhiAX() {
  return OperationSpec::binaryAX("mulhi_ax",
                                 [](uint8_t A, uint8_t X) { return uint8_t((A * X) >> 8); });
}

} // namespace ops

//===----------------------------------------------------------------------===//
// Verifier
//===----------------------------------------------------------------------===//

/// Result of verification.
struct VerifyResult {
  bool Success = false;
  std::optional<uint8_t> FailingInputA;
  std::optional<uint8_t> FailingInputX;
  uint8_t ExpectedOutput = 0;
  uint8_t ActualOutput = 0;
  std::string ErrorMessage;
};

/// Verifies instruction sequences against operation specifications.
class Verifier {
public:
  /// Create a verifier using the given MOS context for execution.
  /// The context must remain valid for the lifetime of the verifier.
  Verifier(llvm::MOS::Context &Ctx, const MCInstrInfo &MII);

  /// Verify that a sequence implements the given operation.
  /// For i8 operations, this exhaustively tests all inputs.
  VerifyResult verify(ArrayRef<MCInst> Seq, const OperationSpec &Spec);

  /// Get the number of test cases run in the last verify() call.
  uint64_t getLastTestCount() const { return LastTestCount; }

private:
  llvm::MOS::Context &Ctx;
  const MCInstrInfo &MII;
  uint64_t LastTestCount = 0;

  /// Test a single input and return whether it passed.
  bool testSingleInput(ArrayRef<MCInst> Seq, const OperationSpec &Spec,
                       uint8_t InputA, uint8_t InputX, VerifyResult &Result);

  /// Execute a sequence on the context.
  void executeSequence(ArrayRef<MCInst> Seq);

  /// Reset context state for a new test.
  void resetState();
};

//===----------------------------------------------------------------------===//
// Convenience Functions
//===----------------------------------------------------------------------===//

/// Find the optimal sequence for an operation.
/// Returns empty vector if no valid sequence found within budget.
/// Note: Ctx must actually be a MOS::Context (will be cast internally).
std::vector<MCInst> findOptimal(const OperationSpec &Spec,
                                llvm::emu::Context &Ctx,
                                const llvm::MCInstrInfo &MII,
                                const llvm::MCSubtargetInfo &STI,
                                unsigned MaxLength = 5,
                                bool Verbose = false);

} // namespace superopt
} // namespace llvm

#endif // LLVM_TARGET_MOS_SUPEROPT_VERIFIER_H
