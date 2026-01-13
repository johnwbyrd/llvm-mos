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
#include "llvm/Emulator/Semihost.h"
#include <memory>
#include <sys/types.h>
#include <vector>

namespace llvm {
namespace emu {

/// A memory-mapped device region.
struct DeviceRegion {
  uint64_t Start; ///< Start address (inclusive)
  uint64_t End;   ///< End address (inclusive)
  Device *Dev;    ///< Device handling this region
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
  uint16_t read16(uint64_t Addr) { return read(Addr) | (read(Addr + 1) << 8); }

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

  //===--------------------------------------------------------------------===//
  // Timer Support
  //===--------------------------------------------------------------------===//

  /// Set the semihost device for timer IRQ coordination.
  /// When the timer fires, System will call SemihostDev->setTimerTick()
  /// to set STATUS=1 before asserting the IRQ line.
  void setSemihostDevice(Semihost *Dev) { SemihostDev = Dev; }

  /// Configure the timer interrupt rate.
  /// @param RateHz Timer frequency in Hz (0 = disable timer).
  /// @param ContextIndex Which CPU context to send IRQs to.
  void configureTimer(unsigned RateHz, size_t ContextIndex = 0) {
    TimerRateHz = RateHz;
    TimerContextIndex = ContextIndex;
    if (RateHz > 0 && ContextIndex < Contexts.size()) {
      // Calculate cycles between IRQs based on CPU clock rate
      uint64_t ClockHz = Contexts[ContextIndex].ClockHz;
      TimerPeriodCycles = ClockHz / RateHz;
      TimerNextFireCycle =
          Contexts[ContextIndex].Ctx->getCycles() + TimerPeriodCycles;
    } else {
      TimerPeriodCycles = 0;
      TimerNextFireCycle = 0;
    }
  }

  /// Set maximum cycles to run before stopping (0 = unlimited).
  void setMaxCycles(uint64_t Max) { MaxCycles = Max; }

  //===--------------------------------------------------------------------===//
  // Execution
  //===--------------------------------------------------------------------===//

  /// Run all CPUs until they all halt or cycle limit is reached.
  /// For single-CPU systems, this runs with timer and cycle limit support.
  bool run() {
    if (Contexts.size() == 1) {
      // Single-CPU path with timer support
      Context *Ctx = Contexts[0].Ctx;
      while (!Ctx->isHalted()) {
        // Check cycle limit
        if (MaxCycles > 0 && Ctx->getCycles() >= MaxCycles)
          return true; // Stopped due to cycle limit (not an error)

        // Check timer before each step
        if (TimerPeriodCycles > 0 && Ctx->getCycles() >= TimerNextFireCycle) {
          // Set STATUS=1 in semihost device (like MAME does)
          if (SemihostDev)
            SemihostDev->setTimerTick();
          // Assert IRQ - stays asserted until guest clears STATUS
          Ctx->assertIRQ();
          TimerNextFireCycle += TimerPeriodCycles;
        }
        if (!Ctx->step())
          return false;
        // Note: IRQ is NOT deasserted here. It stays asserted until
        // the guest writes 0 to STATUS, which triggers deassertIRQ().
        // This matches MAME's level-triggered IRQ behavior.
      }
      return true;
    }

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

  /// Halt all CPUs with the given exit code.
  void halt(int ExitCode = 0) {
    for (auto &Entry : Contexts) {
      Entry.Ctx->halt(ExitCode);
    }
  }

  /// Get the exit code from the first halted context.
  int getExitCode() const {
    for (const auto &Entry : Contexts) {
      if (Entry.Ctx->isHalted())
        return Entry.Ctx->getExitCode();
    }
    return 0;
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

  // Timer state
  unsigned TimerRateHz = 0;
  size_t TimerContextIndex = 0;
  uint64_t TimerPeriodCycles = 0;
  uint64_t TimerNextFireCycle = 0;

  // Execution limits
  uint64_t MaxCycles = 0; // 0 = unlimited

  // Semihost device for timer IRQ coordination
  Semihost *SemihostDev = nullptr;
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SYSTEM_H
