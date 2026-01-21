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
#include <cstring>
#include <map>
#include <memory>
#include <set>
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
///
/// System is the SINGLE SOURCE OF TRUTH for all debugging state:
/// - Breakpoints (checked before any CPU executes)
/// - Watchpoints (checked on all memory access)
/// - Stop reasons (which CPU stopped, why, at what address)
/// - Undo journal (for reverse debugging)
class System {
public:
  //===--------------------------------------------------------------------===//
  // Stop Reason Reporting
  //===--------------------------------------------------------------------===//

  /// Why did execution stop?
  enum class StopReason {
    None,       ///< Still running or not started
    Breakpoint, ///< Hit a breakpoint
    Watchpoint, ///< Hit a watchpoint
    SingleStep, ///< Completed single step
    Halted,     ///< CPU halted (BRK, exit, etc.)
    Error       ///< Execution error
  };

  /// Watchpoint access type.
  enum class WatchType { Read, Write, ReadWrite };

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
  /// Also checks read watchpoints if debugging is active.
  uint8_t read(uint64_t Addr) {
    // Check read watchpoints
    auto It = Watchpoints.find(Addr);
    if (It != Watchpoints.end()) {
      if (It->second.Type == WatchType::Read ||
          It->second.Type == WatchType::ReadWrite) {
        LastStopReason = StopReason::Watchpoint;
        LastStopAddress = Addr;
      }
    }

    // Search in reverse order (later devices have priority)
    for (auto DevIt = Devices.rbegin(); DevIt != Devices.rend(); ++DevIt) {
      if (Addr >= DevIt->Start && Addr <= DevIt->End)
        return DevIt->Dev->read(Addr - DevIt->Start);
    }
    return 0xFF; // Unmapped reads return 0xFF (floating bus)
  }

  /// Write a byte to the given address.
  /// Routes to the appropriate device based on address mapping.
  /// Also logs the write for undo and checks watchpoints.
  void write(uint64_t Addr, uint8_t Value) {
    // Log for undo journal (before the write)
    if (RecordingEnabled)
      UndoLog.push_back({UndoRecord::Memory, Addr, read(Addr), 1});

    // Check write watchpoints
    auto It = Watchpoints.find(Addr);
    if (It != Watchpoints.end()) {
      if (It->second.Type == WatchType::Write ||
          It->second.Type == WatchType::ReadWrite) {
        LastStopReason = StopReason::Watchpoint;
        LastStopAddress = Addr;
      }
    }

    // Search in reverse order (later devices have priority)
    for (auto DevIt = Devices.rbegin(); DevIt != Devices.rend(); ++DevIt) {
      if (Addr >= DevIt->Start && Addr <= DevIt->End) {
        DevIt->Dev->write(Addr - DevIt->Start, Value);
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
    size_t Idx = Contexts.size();
    Ctx->setSystem(this, Idx);
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

  /// Run all CPUs until they all halt, hit a breakpoint/watchpoint, or cycle
  /// limit is reached. For single-CPU systems, runs with timer support.
  /// Returns false if stopped due to breakpoint/watchpoint/error.
  bool run() {
    if (Contexts.size() == 1) {
      // Single-CPU path with timer support
      Context *Ctx = Contexts[0].Ctx;
      while (!Ctx->isHalted()) {
        // Check cycle limit
        if (MaxCycles > 0 && Ctx->getCycles() >= MaxCycles)
          return true; // Stopped due to cycle limit (not an error)

        // Check breakpoints before executing
        if (hasBreakpoint(Ctx->getPC())) {
          LastStopReason = StopReason::Breakpoint;
          LastStopAddress = Ctx->getPC();
          StoppedContext = Ctx;
          return false; // Stopped at breakpoint
        }

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

        // Check if a watchpoint was hit during the step
        if (LastStopReason == StopReason::Watchpoint) {
          StoppedContext = Ctx;
          return false; // Stopped at watchpoint
        }
        // Note: IRQ is NOT deasserted here. It stays asserted until
        // the guest writes 0 to STATUS, which triggers deassertIRQ().
        // This matches MAME's level-triggered IRQ behavior.
      }
      // CPU halted normally
      LastStopReason = StopReason::Halted;
      StoppedContext = Ctx;
      return true;
    }

    // Multi-CPU scheduling with breakpoint/watchpoint support
    for (auto &Entry : Contexts) {
      Context *Ctx = Entry.Ctx;
      while (!Ctx->isHalted()) {
        // Check breakpoints
        if (hasBreakpoint(Ctx->getPC())) {
          LastStopReason = StopReason::Breakpoint;
          LastStopAddress = Ctx->getPC();
          StoppedContext = Ctx;
          return false;
        }
        if (!Ctx->step())
          return false;
        // Check watchpoints
        if (LastStopReason == StopReason::Watchpoint) {
          StoppedContext = Ctx;
          return false;
        }
      }
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

  //===--------------------------------------------------------------------===//
  // Breakpoint Support (system-wide)
  //===--------------------------------------------------------------------===//

  /// Add a software breakpoint at the given address.
  /// Checked before any CPU executes at that address.
  bool addBreakpoint(uint64_t Addr) {
    return Breakpoints.insert(Addr).second;
  }

  /// Remove a breakpoint at the given address.
  bool removeBreakpoint(uint64_t Addr) { return Breakpoints.erase(Addr) > 0; }

  /// Check if there is a breakpoint at the given address.
  bool hasBreakpoint(uint64_t Addr) const {
    return Breakpoints.count(Addr) > 0;
  }

  //===--------------------------------------------------------------------===//
  // Watchpoint Support (checked on all memory access)
  //===--------------------------------------------------------------------===//

  /// Add a watchpoint at the given address range.
  bool addWatchpoint(uint64_t Addr, size_t Size, WatchType Type) {
    Watchpoints[Addr] = {Size, Type};
    return true;
  }

  /// Remove a watchpoint at the given address.
  bool removeWatchpoint(uint64_t Addr) { return Watchpoints.erase(Addr) > 0; }

  //===--------------------------------------------------------------------===//
  // Stop Reason Access
  //===--------------------------------------------------------------------===//

  /// Get the reason execution stopped.
  StopReason getStopReason() const { return LastStopReason; }

  /// Get the address where execution stopped (breakpoint/watchpoint address).
  uint64_t getStopAddress() const { return LastStopAddress; }

  /// Get which CPU context stopped (for multi-CPU systems).
  Context *getStoppedContext() const { return StoppedContext; }

  /// Clear the stop reason (call before resuming execution).
  void clearStopReason() {
    LastStopReason = StopReason::None;
    LastStopAddress = 0;
    StoppedContext = nullptr;
  }

  //===--------------------------------------------------------------------===//
  // Undo Journal (for reverse debugging)
  //===--------------------------------------------------------------------===//

  /// Create a checkpoint at the current position.
  /// Checkpoints are lightweight - just save the undo log position.
  void checkpoint() {
    uint64_t Cycles = 0;
    if (!Contexts.empty())
      Cycles = Contexts[0].Ctx->getCycles();
    Checkpoints.push_back({UndoLog.size(), Cycles});
  }

  /// Restore to a previous checkpoint.
  /// Replays the undo log backwards to restore state.
  bool restoreToCheckpoint(size_t Idx);

  /// Get the number of checkpoints.
  size_t getCheckpointCount() const { return Checkpoints.size(); }

  /// Enable or disable undo logging.
  /// When enabled, all memory writes are logged.
  void enableRecording(bool Enable) { RecordingEnabled = Enable; }

  /// Check if recording is enabled.
  bool isRecording() const { return RecordingEnabled; }

  /// Record a register write (called by Context before modifying a register).
  /// @param CtxIdx Index of the context in this system.
  /// @param RegNum Register number within that context.
  /// @param OldValue Pointer to the old value.
  /// @param Size Size of the register in bytes.
  void recordRegisterWrite(size_t CtxIdx, unsigned RegNum, const void *OldValue,
                           size_t Size) {
    if (!RecordingEnabled)
      return;
    uint64_t Key = (uint64_t(CtxIdx) << 32) | RegNum;
    uint64_t Val = 0;
    std::memcpy(&Val, OldValue, Size);
    UndoLog.push_back({UndoRecord::Register, Key, Val, uint8_t(Size)});
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

  //===--------------------------------------------------------------------===//
  // Debugging State (single source of truth)
  //===--------------------------------------------------------------------===//

  // Breakpoints - set of addresses where execution should stop
  std::set<uint64_t> Breakpoints;

  // Watchpoints - map from address to {size, type}
  struct WatchInfo {
    size_t Size;
    WatchType Type;
  };
  std::map<uint64_t, WatchInfo> Watchpoints;

  // Stop reason from last execution
  StopReason LastStopReason = StopReason::None;
  uint64_t LastStopAddress = 0;
  Context *StoppedContext = nullptr;

  // Undo journal for reverse debugging
  struct UndoRecord {
    enum Kind : uint8_t { Memory, Register };
    Kind Type;
    uint64_t Key;      ///< Address for memory, (CtxIdx << 32 | RegNum) for reg
    uint64_t OldValue; ///< Old value (fits any register up to 64-bit)
    uint8_t Size;      ///< Size in bytes
  };
  std::vector<UndoRecord> UndoLog;
  bool RecordingEnabled = false;

  struct Checkpoint {
    size_t UndoLogPosition;
    uint64_t CycleCount;
  };
  std::vector<Checkpoint> Checkpoints;

  /// Route a write without logging (used during restore).
  void routeWriteNoLog(uint64_t Addr, uint8_t Value) {
    for (auto It = Devices.rbegin(); It != Devices.rend(); ++It) {
      if (Addr >= It->Start && Addr <= It->End) {
        It->Dev->write(Addr - It->Start, Value);
        return;
      }
    }
  }
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SYSTEM_H
