//===-- llvm/Emulator/System.h - Multi-CPU Emulation System -----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Coordinates CPU execution and routes memory access to devices.
///
/// System is the central coordinator for emulation. It maintains a list of
/// memory-mapped devices and routes all memory accesses through them. Devices
/// are checked in reverse order of registration, so later-added devices can
/// overlay earlier ones (useful for ROM overlays, I/O windows, etc.).
///
/// For debugging, System is the single source of truth for:
/// - Breakpoints and watchpoints (checked on every access/step)
/// - Stop reasons (why execution halted)
/// - Undo journal (for reverse debugging via checkpoints)
///
/// The undo journal records memory and register writes, allowing restoration
/// to previous checkpoints by replaying writes in reverse. Checkpoints are
/// lightweight markers into the undo log, not full snapshots.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SYSTEM_H
#define LLVM_EMULATOR_SYSTEM_H

#include "llvm/Emulator/Context.h"
#include "llvm/Emulator/Device.h"
#include "llvm/Emulator/Memory.h"
#include "llvm/Emulator/Semihost.h"
#include "llvm/Support/Endian.h"
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace llvm {
namespace emu {

/// \brief Coordinates CPU execution and routes memory access to devices.
///
/// Not thread-safe. All calls must be from the same thread.
class System {
public:
  enum class StopReason {
    None,
    Breakpoint,
    Watchpoint,
    SingleStep,
    Halted,
    Error
  };

  enum class WatchType { Read, Write, ReadWrite };

  System() = default;
  ~System() = default;

  System(const System &) = delete;
  System &operator=(const System &) = delete;

  /// \brief Create a system with memory and semihosting pre-configured.
  ///
  /// Memory covers the full address space. Semihost is placed at the
  /// ZBC-specified address near the top of memory.
  ///
  /// \param AddrBits Address bus width (e.g., 16 for 64KB).
  /// \param SandboxDir If non-empty, restricts file I/O to this directory.
  static std::unique_ptr<System> create(unsigned AddrBits,
                                        const std::string &SandboxDir = "");

  Memory *getMemory() { return Mem; }
  Semihost *getSemihost() { return SemihostDev; }

  //===--------------------------------------------------------------------===//
  // Device Management
  //===--------------------------------------------------------------------===//

  /// \brief Add a device to the memory map. System takes ownership.
  ///
  /// Later-added devices shadow earlier ones at overlapping addresses.
  void addDevice(uint64_t Start, uint64_t End, std::unique_ptr<Device> Dev);

  //===--------------------------------------------------------------------===//
  // Memory Access
  //===--------------------------------------------------------------------===//

  /// \brief Read a byte, checking watchpoints and routing to devices.
  uint8_t read(uint64_t Addr);

  /// \brief Write a byte, logging for undo, checking watchpoints.
  void write(uint64_t Addr, uint8_t Value);

  /// \brief Read an integer of arbitrary size.
  /// \param Size Bytes to read (1, 2, 4, or 8).
  uint64_t readN(uint64_t Addr, unsigned Size,
                 llvm::endianness Endian = llvm::endianness::little);

  /// \brief Write an integer of arbitrary size.
  void writeN(uint64_t Addr, uint64_t Value, unsigned Size,
              llvm::endianness Endian = llvm::endianness::little);

  //===--------------------------------------------------------------------===//
  // CPU Management
  //===--------------------------------------------------------------------===//

  /// \brief Register a CPU context. System does not take ownership.
  void addContext(Context *Ctx, uint64_t ClockHz = 1000000);

  size_t getContextCount() const { return Contexts.size(); }

  Context *getContext(size_t Index) const {
    return Index < Contexts.size() ? Contexts[Index].Ctx : nullptr;
  }

  //===--------------------------------------------------------------------===//
  // Timer
  //===--------------------------------------------------------------------===//

  void setSemihostDevice(Semihost *Dev) { SemihostDev = Dev; }

  /// \brief Configure periodic timer IRQs.
  /// \param RateHz Frequency in Hz, or 0 to disable.
  void configureTimer(unsigned RateHz, size_t ContextIndex = 0);

  void setMaxCycles(uint64_t Max) { MaxCycles = Max; }

  //===--------------------------------------------------------------------===//
  // Execution Control
  //===--------------------------------------------------------------------===//

  /// \brief Run until halt, breakpoint, watchpoint, or cycle limit.
  /// \return true if halted normally or hit cycle limit; false if stopped
  ///         by breakpoint/watchpoint/error.
  bool run();

  /// \brief Single-step forward. Creates checkpoint if recording.
  void step();

  /// \brief Step backward one checkpoint.
  /// \return false if at beginning of history.
  bool stepReverse();

  /// \brief Run backward until breakpoint or beginning of history.
  bool runReverse();

  bool isAtHistoryBoundary() const { return AtHistoryBoundary; }

  void reset();
  bool allHalted() const;
  void halt(int ExitCode = 0);
  int getExitCode() const;

  //===--------------------------------------------------------------------===//
  // Breakpoints and Watchpoints
  //===--------------------------------------------------------------------===//

  bool addBreakpoint(uint64_t Addr) { return Breakpoints.insert(Addr).second; }
  bool removeBreakpoint(uint64_t Addr) { return Breakpoints.erase(Addr) > 0; }
  bool hasBreakpoint(uint64_t Addr) const {
    return Breakpoints.count(Addr) > 0;
  }

  bool addWatchpoint(uint64_t Addr, size_t Size, WatchType Type) {
    Watchpoints[Addr] = {Size, Type};
    return true;
  }
  bool removeWatchpoint(uint64_t Addr) { return Watchpoints.erase(Addr) > 0; }

  //===--------------------------------------------------------------------===//
  // Stop Reason
  //===--------------------------------------------------------------------===//

  StopReason getStopReason() const { return LastStopReason; }
  uint64_t getStopAddress() const { return LastStopAddress; }
  Context *getStoppedContext() const { return StoppedContext; }

  void clearStopReason() {
    LastStopReason = StopReason::None;
    LastStopAddress = 0;
    StoppedContext = nullptr;
  }

  void setStopReason(StopReason Reason, uint64_t Addr = 0) {
    LastStopReason = Reason;
    LastStopAddress = Addr;
  }

  //===--------------------------------------------------------------------===//
  // Undo Journal
  //===--------------------------------------------------------------------===//

  void checkpoint();
  bool restoreToCheckpoint(size_t Idx);
  size_t getCheckpointCount() const { return Checkpoints.size(); }

  void enableRecording(bool Enable) { RecordingEnabled = Enable; }
  bool isRecording() const { return RecordingEnabled; }

  /// \brief Called by Context before modifying a register.
  void recordRegisterWrite(size_t CtxIdx, unsigned RegNum, const void *OldValue,
                           size_t Size);

private:
  struct ContextEntry {
    Context *Ctx;
    uint64_t ClockHz;
    uint64_t LocalCycles;
    bool Suspended;
  };

  struct DeviceRegion {
    uint64_t Start;
    uint64_t End;
    std::unique_ptr<Device> Dev;
  };

  std::vector<ContextEntry> Contexts;
  std::vector<DeviceRegion> Devices;

  // Timer state
  unsigned TimerRateHz = 0;
  size_t TimerContextIndex = 0;
  uint64_t TimerPeriodCycles = 0;
  uint64_t TimerNextFireCycle = 0;

  uint64_t MaxCycles = 0;

  // Standard devices (set by create())
  Memory *Mem = nullptr;
  Semihost *SemihostDev = nullptr;

  // Breakpoints and watchpoints
  std::set<uint64_t> Breakpoints;

  struct WatchInfo {
    size_t Size;
    WatchType Type;
  };
  std::map<uint64_t, WatchInfo> Watchpoints;

  // Stop state
  StopReason LastStopReason = StopReason::None;
  uint64_t LastStopAddress = 0;
  Context *StoppedContext = nullptr;

  // Undo journal
  struct UndoRecord {
    enum Kind : uint8_t { Memory, Register };
    Kind Type;
    uint64_t Key; // Address for memory, (CtxIdx << 32 | RegNum) for reg
    uint64_t OldValue;
    uint8_t Size;
  };
  std::vector<UndoRecord> UndoLog;
  bool RecordingEnabled = false;

  struct Checkpoint {
    size_t UndoLogPosition;
    uint64_t CycleCount;
  };
  std::vector<Checkpoint> Checkpoints;

  bool AtHistoryBoundary = false;

  // Helpers
  Device *findDevice(uint64_t Addr, uint64_t &Offset) const;
  void routeWrite(uint64_t Addr, uint8_t Value, bool Log);
  bool checkBreakpointStop(Context *Ctx);
  bool checkWatchpointStop(Context *Ctx);
  void checkAndFireTimer(Context *Ctx);
  bool runSingleCPU();
  bool runMultiCPU();
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SYSTEM_H
