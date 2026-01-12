//===-- llvm/Emulator/Memory.h - RAM/ROM Device -----------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a simple RAM/ROM device for the emulator.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_MEMORY_H
#define LLVM_EMULATOR_MEMORY_H

#include "llvm/Emulator/Device.h"
#include <cstring>
#include <memory>
#include <vector>

namespace llvm {
namespace emu {

/// A simple RAM/ROM memory device.
/// Provides a byte array with optional read-only flag.
class Memory : public Device {
public:
  /// Create a memory device of the given size, initialized to zero.
  explicit Memory(uint64_t Size, bool ReadOnly = false)
      : Data(Size, 0), IsReadOnly(ReadOnly) {}

  /// Create a memory device from existing data.
  Memory(const uint8_t *Src, uint64_t Size, bool ReadOnly = false)
      : Data(Src, Src + Size), IsReadOnly(ReadOnly) {}

  uint8_t read(uint64_t Offset) override {
    if (Offset >= Data.size())
      return 0xFF; // Unmapped reads return 0xFF
    return Data[Offset];
  }

  void write(uint64_t Offset, uint8_t Value) override {
    if (IsReadOnly || Offset >= Data.size())
      return;
    Data[Offset] = Value;
  }

  void readBlock(uint8_t *Dest, uint64_t Offset, uint64_t Size) override {
    if (Offset >= Data.size()) {
      std::memset(Dest, 0xFF, Size);
      return;
    }
    uint64_t Available = Data.size() - Offset;
    uint64_t ToCopy = std::min(Size, Available);
    std::memcpy(Dest, Data.data() + Offset, ToCopy);
    if (ToCopy < Size)
      std::memset(Dest + ToCopy, 0xFF, Size - ToCopy);
  }

  void writeBlock(uint64_t Offset, const uint8_t *Src, uint64_t Size) override {
    if (IsReadOnly || Offset >= Data.size())
      return;
    uint64_t Available = Data.size() - Offset;
    uint64_t ToCopy = std::min(Size, Available);
    std::memcpy(Data.data() + Offset, Src, ToCopy);
  }

  /// Get the size of this memory device.
  uint64_t size() const { return Data.size(); }

  /// Get direct access to the underlying data.
  uint8_t *data() { return Data.data(); }
  const uint8_t *data() const { return Data.data(); }

  /// Check if this memory is read-only.
  bool isReadOnly() const { return IsReadOnly; }

private:
  std::vector<uint8_t> Data;
  bool IsReadOnly;
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_MEMORY_H
