//===-- llvm/Emulator/System.h - Multi-CPU Scheduler ------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the System class which coordinates multiple CPUs and
// manages shared memory-mapped devices.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SYSTEM_H
#define LLVM_EMULATOR_SYSTEM_H

#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/Device.h"
#include <memory>
#include <vector>

namespace llvm {
namespace emu {

/// A memory-mapped device region.
struct DeviceRegion {
  uint64_t Start;  ///< Start address (inclusive)
  uint64_t End;    ///< End address (inclusive)
  Device *Dev;     ///< Device handling this region
};

/// Coordinates multiple CPUs and manages shared devices.
/// For single-CPU systems, provides device routing without scheduling overhead.
class System {
public:
  System() = default;
  ~System() = default;

  // Non-copyable
  System(const System &) = delete;
  System &operator=(const System &) = delete;

  //===--------------------------------------------------------------------===//
  // Device Management
  //===--------------------------------------------------------------------===//

  /// Add a device mapped to the given address range.
  /// Later-added devices take priority for overlapping regions.
  void addDevice(uint64_t Start, uint64_t End, Device *Dev) {
    Devices.push_back({Start, End, Dev});
  }

  /// Add a device that owns its memory (will be deleted with the system).
  void addOwnedDevice(uint64_t Start, uint64_t End,
                      std::unique_ptr<Device> Dev) {
    Devices.push_back({Start, End, Dev.get()});
    OwnedDevices.push_back(std::move(Dev));
  }

  //===--------------------------------------------------------------------===//
  // Memory Access
  //===--------------------------------------------------------------------===//

  /// Read a byte from the given address.
  /// Routes to the appropriate device based on address mapping.
  uint8_t read(uint64_t Addr) {
    // Search in reverse order (later devices have priority)
    for (auto It = Devices.rbegin(); It != Devices.rend(); ++It) {
      if (Addr >= It->Start && Addr <= It->End)
        return It->Dev->read(Addr - It->Start);
    }
    return 0xFF; // Unmapped reads return 0xFF (floating bus)
  }

  /// Write a byte to the given address.
  /// Routes to the appropriate device based on address mapping.
  void write(uint64_t Addr, uint8_t Value) {
    // Search in reverse order (later devices have priority)
    for (auto It = Devices.rbegin(); It != Devices.rend(); ++It) {
      if (Addr >= It->Start && Addr <= It->End) {
        It->Dev->write(Addr - It->Start, Value);
        return;
      }
    }
    // Unmapped writes are ignored
  }

  /// Read a 16-bit little-endian value from the given address.
  uint16_t read16(uint64_t Addr) {
    return read(Addr) | (read(Addr + 1) << 8);
  }

  /// Write a 16-bit little-endian value to the given address.
  void write16(uint64_t Addr, uint16_t Value) {
    write(Addr, Value & 0xFF);
    write(Addr + 1, Value >> 8);
  }

  //===--------------------------------------------------------------------===//
  // CPU Management
  //===--------------------------------------------------------------------===//

  /// Add a CPU context to the system.
  void addContext(Context *Ctx, uint64_t ClockHz = 1000000) {
    Ctx->setSystem(this);
    Contexts.push_back({Ctx, ClockHz, 0, false});
  }

  /// Get the number of CPU contexts.
  size_t getContextCount() const { return Contexts.size(); }

  /// Get a CPU context by index.
  Context *getContext(size_t Index) const {
    return Index < Contexts.size() ? Contexts[Index].Ctx : nullptr;
  }

  //===--------------------------------------------------------------------===//
  // Execution
  //===--------------------------------------------------------------------===//

  /// Run all CPUs until they all halt.
  /// For single-CPU systems, this just calls run() on the context.
  bool run() {
    if (Contexts.size() == 1)
      return Contexts[0].Ctx->run();

    // Multi-CPU scheduling would go here
    // For now, just run them sequentially
    for (auto &Entry : Contexts) {
      if (!Entry.Ctx->run())
        return false;
    }
    return true;
  }

  /// Check if all CPUs have halted.
  bool allHalted() const {
    for (const auto &Entry : Contexts) {
      if (!Entry.Ctx->isHalted())
        return false;
    }
    return true;
  }

private:
  struct ContextEntry {
    Context *Ctx;
    uint64_t ClockHz;
    uint64_t LocalCycles;
    bool Suspended;
  };

  std::vector<ContextEntry> Contexts;
  std::vector<DeviceRegion> Devices;
  std::vector<std::unique_ptr<Device>> OwnedDevices;
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SYSTEM_H
