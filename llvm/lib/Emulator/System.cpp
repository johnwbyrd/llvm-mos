//===-- System.cpp - Multi-CPU System Implementation -----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/System.h"

#include "llvm/Emulator/Semihost/Policy.h"

#include <cstring>

using namespace llvm;
using namespace llvm::emu;
using namespace llvm::emu::semihost;

//===----------------------------------------------------------------------===//
// Factory
//===----------------------------------------------------------------------===//

std::unique_ptr<System> System::create(unsigned AddrBits,
                                       const std::string &SandboxDir) {
  auto Sys = std::make_unique<System>();

  // Cap memory at 4GB for large address spaces
  uint64_t MemSize = (AddrBits >= 32) ? (4ULL * 1024 * 1024 * 1024)
                                      : (1ULL << AddrBits);

  // Create and add memory
  auto MemDev = std::make_unique<Memory>(MemSize);
  Sys->Mem = MemDev.get();
  Sys->addDevice(0, MemSize - 1, std::move(MemDev));

  // Derive default platform config from address bits
  uint8_t PtrSize = (AddrBits + 7) / 8;
  semihost::PlatformConfig Config(PtrSize, PtrSize, llvm::endianness::little);

  // Create security policy
  std::unique_ptr<Policy> SemihostPolicy;
  if (!SandboxDir.empty())
    SemihostPolicy = std::make_unique<SandboxedPolicy>(SandboxDir);
  else
    SemihostPolicy = std::make_unique<ConsoleOnlyPolicy>();

  // Create and add semihost at ZBC-specified address
  auto SemihostDev = Semihost::create(*Sys, Config, std::move(SemihostPolicy));
  uint64_t ReservedStart = MemSize - (1ULL << (AddrBits / 2));
  uint64_t SemihostBase = ReservedStart - 512 - 32;

  Sys->SemihostDev = SemihostDev.get();
  Sys->addDevice(SemihostBase, SemihostBase + 31, std::move(SemihostDev));

  // Set exit callback to halt the system
  Sys->SemihostDev->setExitCallback(
      [SysPtr = Sys.get()](unsigned, unsigned Subcode) {
        SysPtr->halt(static_cast<int>(Subcode));
      });

  return Sys;
}

//===----------------------------------------------------------------------===//
// Device Management
//===----------------------------------------------------------------------===//

void System::addDevice(uint64_t Start, uint64_t End,
                       std::unique_ptr<Device> Dev) {
  Devices.push_back({Start, End, std::move(Dev)});
}

Device *System::findDevice(uint64_t Addr, uint64_t &Offset) const {
  // Search in reverse order - later devices shadow earlier ones
  for (auto It = Devices.rbegin(); It != Devices.rend(); ++It) {
    if (Addr >= It->Start && Addr <= It->End) {
      Offset = Addr - It->Start;
      return It->Dev.get();
    }
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Memory Access
//===----------------------------------------------------------------------===//

uint8_t System::read(uint64_t Addr) {
  // Check read watchpoints
  auto It = Watchpoints.find(Addr);
  if (It != Watchpoints.end()) {
    if (It->second.Type == WatchType::Read ||
        It->second.Type == WatchType::ReadWrite) {
      LastStopReason = StopReason::Watchpoint;
      LastStopAddress = Addr;
    }
  }

  uint64_t Offset;
  if (Device *Dev = findDevice(Addr, Offset))
    return Dev->read(Offset);
  return 0xFF; // Floating bus
}

void System::write(uint64_t Addr, uint8_t Value) {
  routeWrite(Addr, Value, true);
}

void System::routeWrite(uint64_t Addr, uint8_t Value, bool Log) {
  // Log for undo journal before the write
  if (Log && RecordingEnabled) {
    uint64_t Offset;
    uint8_t OldValue = 0xFF;
    if (Device *Dev = findDevice(Addr, Offset))
      OldValue = Dev->read(Offset);
    UndoLog.push_back({UndoRecord::Memory, Addr, OldValue, 1});
  }

  // Check write watchpoints
  auto It = Watchpoints.find(Addr);
  if (It != Watchpoints.end()) {
    if (It->second.Type == WatchType::Write ||
        It->second.Type == WatchType::ReadWrite) {
      LastStopReason = StopReason::Watchpoint;
      LastStopAddress = Addr;
    }
  }

  uint64_t Offset;
  if (Device *Dev = findDevice(Addr, Offset))
    Dev->write(Offset, Value);
}

uint64_t System::readN(uint64_t Addr, unsigned Size, llvm::endianness Endian) {
  uint64_t Value = 0;
  if (Endian == llvm::endianness::little) {
    for (unsigned I = 0; I < Size; ++I)
      Value |= uint64_t(read(Addr + I)) << (I * 8);
  } else {
    for (unsigned I = 0; I < Size; ++I)
      Value = (Value << 8) | read(Addr + I);
  }
  return Value;
}

void System::writeN(uint64_t Addr, uint64_t Value, unsigned Size,
                    llvm::endianness Endian) {
  if (Endian == llvm::endianness::little) {
    for (unsigned I = 0; I < Size; ++I) {
      write(Addr + I, Value & 0xFF);
      Value >>= 8;
    }
  } else {
    for (unsigned I = Size; I > 0; --I) {
      write(Addr + I - 1, Value & 0xFF);
      Value >>= 8;
    }
  }
}

//===----------------------------------------------------------------------===//
// CPU Management
//===----------------------------------------------------------------------===//

void System::addContext(Context *Ctx, uint64_t ClockHz) {
  size_t Idx = Contexts.size();
  Ctx->setSystem(this, Idx);
  Contexts.push_back({Ctx, ClockHz, 0, false});
}

void System::configureTimer(unsigned RateHz, size_t ContextIndex) {
  TimerRateHz = RateHz;
  TimerContextIndex = ContextIndex;
  if (RateHz > 0 && ContextIndex < Contexts.size()) {
    uint64_t ClockHz = Contexts[ContextIndex].ClockHz;
    TimerPeriodCycles = ClockHz / RateHz;
    TimerNextFireCycle =
        Contexts[ContextIndex].Ctx->getCycles() + TimerPeriodCycles;
  } else {
    TimerPeriodCycles = 0;
    TimerNextFireCycle = 0;
  }
}

//===----------------------------------------------------------------------===//
// Execution Control - Helpers
//===----------------------------------------------------------------------===//

bool System::checkBreakpointStop(Context *Ctx) {
  if (!hasBreakpoint(Ctx->getPC()))
    return false;

  LastStopReason = StopReason::Breakpoint;
  LastStopAddress = Ctx->getPC();
  StoppedContext = Ctx;
  if (RecordingEnabled)
    checkpoint();
  return true;
}

bool System::checkWatchpointStop(Context *Ctx) {
  if (LastStopReason != StopReason::Watchpoint)
    return false;

  StoppedContext = Ctx;
  if (RecordingEnabled)
    checkpoint();
  return true;
}

void System::checkAndFireTimer(Context *Ctx) {
  if (TimerPeriodCycles == 0 || Ctx->getCycles() < TimerNextFireCycle)
    return;

  if (SemihostDev)
    SemihostDev->setTimerTick();
  Ctx->assertIRQ();
  TimerNextFireCycle += TimerPeriodCycles;
}

//===----------------------------------------------------------------------===//
// Execution Control
//===----------------------------------------------------------------------===//

bool System::run() {
  return Contexts.size() == 1 ? runSingleCPU() : runMultiCPU();
}

bool System::runSingleCPU() {
  Context *Ctx = Contexts[0].Ctx;

  while (!Ctx->isHalted()) {
    if (MaxCycles > 0 && Ctx->getCycles() >= MaxCycles)
      return true;

    if (checkBreakpointStop(Ctx))
      return false;

    checkAndFireTimer(Ctx);

    if (!Ctx->step())
      return false;

    if (checkWatchpointStop(Ctx))
      return false;
  }

  LastStopReason = StopReason::Halted;
  StoppedContext = Ctx;
  if (RecordingEnabled)
    checkpoint();
  return true;
}

bool System::runMultiCPU() {
  for (auto &Entry : Contexts) {
    Context *Ctx = Entry.Ctx;
    while (!Ctx->isHalted()) {
      if (checkBreakpointStop(Ctx))
        return false;

      if (!Ctx->step())
        return false;

      if (checkWatchpointStop(Ctx))
        return false;
    }
  }

  if (RecordingEnabled)
    checkpoint();
  return true;
}

void System::step() {
  clearStopReason();
  AtHistoryBoundary = false;

  for (auto &Entry : Contexts) {
    if (!Entry.Ctx->isHalted())
      Entry.Ctx->step();
  }

  for (auto &Entry : Contexts) {
    if (!Entry.Ctx->isHalted()) {
      setStopReason(StopReason::SingleStep, Entry.Ctx->getPC());
      StoppedContext = Entry.Ctx;
      break;
    }
  }

  if (RecordingEnabled)
    checkpoint();
}

bool System::stepReverse() {
  AtHistoryBoundary = false;

  if (Checkpoints.size() <= 1) {
    AtHistoryBoundary = true;
    return false;
  }

  Checkpoints.pop_back();
  restoreToCheckpoint(Checkpoints.size() - 1);

  for (auto &Entry : Contexts) {
    setStopReason(StopReason::SingleStep, Entry.Ctx->getPC());
    StoppedContext = Entry.Ctx;
    break;
  }
  return true;
}

bool System::runReverse() {
  AtHistoryBoundary = false;

  while (Checkpoints.size() > 1) {
    Checkpoints.pop_back();
    restoreToCheckpoint(Checkpoints.size() - 1);

    for (auto &Entry : Contexts) {
      if (hasBreakpoint(Entry.Ctx->getPC())) {
        setStopReason(StopReason::Breakpoint, Entry.Ctx->getPC());
        StoppedContext = Entry.Ctx;
        return true;
      }
    }
  }

  AtHistoryBoundary = true;
  return false;
}

void System::reset() {
  for (auto &Entry : Contexts)
    Entry.Ctx->reset();
}

bool System::allHalted() const {
  for (const auto &Entry : Contexts) {
    if (!Entry.Ctx->isHalted())
      return false;
  }
  return true;
}

void System::halt(int ExitCode) {
  for (auto &Entry : Contexts)
    Entry.Ctx->halt(ExitCode);
}

int System::getExitCode() const {
  for (const auto &Entry : Contexts) {
    if (Entry.Ctx->isHalted())
      return Entry.Ctx->getExitCode();
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// Undo Journal
//===----------------------------------------------------------------------===//

void System::checkpoint() {
  uint64_t Cycles = 0;
  if (!Contexts.empty())
    Cycles = Contexts[0].Ctx->getCycles();
  Checkpoints.push_back({UndoLog.size(), Cycles});
}

void System::recordRegisterWrite(size_t CtxIdx, unsigned RegNum,
                                 const void *OldValue, size_t Size) {
  if (!RecordingEnabled)
    return;
  uint64_t Key = (uint64_t(CtxIdx) << 32) | RegNum;
  uint64_t Val = 0;
  std::memcpy(&Val, OldValue, Size);
  UndoLog.push_back({UndoRecord::Register, Key, Val, uint8_t(Size)});
}

bool System::restoreToCheckpoint(size_t Idx) {
  if (Idx >= Checkpoints.size())
    return false;

  auto &CP = Checkpoints[Idx];

  // Replay undo log backwards to restore previous state
  for (size_t I = UndoLog.size(); I > CP.UndoLogPosition; --I) {
    auto &Rec = UndoLog[I - 1];
    if (Rec.Type == UndoRecord::Memory) {
      routeWrite(Rec.Key, uint8_t(Rec.OldValue), false);
    } else {
      size_t CtxIdx = Rec.Key >> 32;
      unsigned RegNum = Rec.Key & 0xFFFFFFFF;
      if (CtxIdx < Contexts.size())
        Contexts[CtxIdx].Ctx->writeRegisterNoLog(RegNum, &Rec.OldValue,
                                                  Rec.Size);
    }
  }

  UndoLog.resize(CP.UndoLogPosition);
  Checkpoints.resize(Idx + 1);

  LastStopReason = StopReason::None;
  LastStopAddress = 0;
  StoppedContext = nullptr;

  return true;
}
