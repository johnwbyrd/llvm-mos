//===-- llvm/Emulator/Device.h - Emulator Device Interface -----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Abstract interface for memory-mapped peripherals.
///
/// CPUs interact with the outside world through memory-mapped I/O: reading
/// and writing to specific addresses that correspond to hardware registers
/// rather than RAM. Device is the abstraction for anything that responds to
/// these accesses - RAM, ROM, I/O controllers, timers, semihosting, etc.
///
/// System maintains a list of devices with address ranges. When the CPU
/// reads or writes, System finds the matching device and calls its read()
/// or write() method with an offset relative to the device's base address.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_DEVICE_H
#define LLVM_EMULATOR_DEVICE_H

#include <cstdint>

namespace llvm {
namespace emu {

/// Abstract interface for memory-mapped devices.
/// Devices respond to read/write operations within their mapped address range.
class Device {
public:
  virtual ~Device() = default;

  /// Read a byte from the device at the given offset.
  /// The offset is relative to the device's mapped base address.
  virtual uint8_t read(uint64_t Offset) = 0;

  /// Write a byte to the device at the given offset.
  /// The offset is relative to the device's mapped base address.
  virtual void write(uint64_t Offset, uint8_t Value) = 0;

  /// Read a block of bytes from the device.
  /// Default implementation calls read() for each byte.
  virtual void readBlock(uint8_t *Dest, uint64_t Offset, uint64_t Size) {
    for (uint64_t I = 0; I < Size; ++I)
      Dest[I] = read(Offset + I);
  }

  /// Write a block of bytes to the device.
  /// Default implementation calls write() for each byte.
  virtual void writeBlock(uint64_t Offset, const uint8_t *Src, uint64_t Size) {
    for (uint64_t I = 0; I < Size; ++I)
      write(Offset + I, Src[I]);
  }
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_DEVICE_H
