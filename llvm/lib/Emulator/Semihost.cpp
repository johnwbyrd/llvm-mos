//===-- Semihost.cpp - Semihosting Device Implementation --------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost.h"
#include "llvm/Emulator/Semihost/FileBackend.h"
#include "llvm/Emulator/System.h"
#include "llvm/Support/Error.h"
#include <cerrno>

using namespace llvm;
using namespace llvm::emu;
using namespace llvm::emu::semihost;

namespace {
constexpr size_t WorkBufferSize = 4096;
} // namespace

//===----------------------------------------------------------------------===//
// Callback Helpers
//===----------------------------------------------------------------------===//

semihost::ExitCallback Semihost::makeExitCallback() {
  return [this](unsigned Reason, unsigned Subcode) {
    Exited = true;
    ExitReason = Reason;
    ExitSubcode = Subcode;
    if (OnExit)
      OnExit(Reason, Subcode);
  };
}

semihost::TimerCallback Semihost::makeTimerCallback(System &Sys) {
  return [&Sys](unsigned RateHz) { Sys.configureTimer(RateHz); };
}

//===----------------------------------------------------------------------===//
// Factory Method
//===----------------------------------------------------------------------===//

std::unique_ptr<Semihost> Semihost::create(System &Sys,
                                           PlatformConfig Config,
                                           std::unique_ptr<Policy> ThePolicy) {
  auto Dev = std::unique_ptr<Semihost>(
      new Semihost(Sys, nullptr, std::move(ThePolicy), Config));

  Dev->TheBackend = std::make_unique<FileBackend>(Dev->makeExitCallback(),
                                                  makeTimerCallback(Sys));
  return Dev;
}

//===----------------------------------------------------------------------===//
// Constructor/Destructor
//===----------------------------------------------------------------------===//

Semihost::Semihost(System &Sys, std::unique_ptr<semihost::Backend> Back,
                   std::unique_ptr<semihost::Policy> Pol,
                   semihost::PlatformConfig Cfg)
    : Sys(Sys), TheBackend(std::move(Back)), ThePolicy(std::move(Pol)),
      Config(Cfg), WorkBuffer(WorkBufferSize) {}

Semihost::~Semihost() = default;

//===----------------------------------------------------------------------===//
// Configuration
//===----------------------------------------------------------------------===//

void Semihost::setExitCallback(semihost::ExitCallback CB) {
  OnExit = std::move(CB);
}

void Semihost::addAllowedPath(const std::string &Prefix, bool AllowWrite) {
  ThePolicy->addAllowedPath(Prefix, AllowWrite);
}

//===----------------------------------------------------------------------===//
// Device Interface
//===----------------------------------------------------------------------===//

uint8_t Semihost::read(uint64_t Offset) {
  if (Offset >= DeviceReg::Size)
    return 0xFF;

  // Signature (8 bytes at offset 0)
  if (Offset < SignatureSize) {
    return static_cast<uint8_t>(Signature[Offset]);
  }

  // RIFF_PTR (8 bytes at offset 0x08, little-endian)
  if (Offset >= DeviceReg::RiffPtr && Offset < DeviceReg::Reserved) {
    unsigned ByteIdx = Offset - DeviceReg::RiffPtr;
    return static_cast<uint8_t>((RiffPtr >> (ByteIdx * 8)) & 0xFF);
  }

  // STATUS (1 byte at offset 0x19)
  if (Offset == DeviceReg::Status) {
    return ResponseReady ? Status::Timer : Status::None;
  }

  return 0x00;
}

void Semihost::write(uint64_t Offset, uint8_t Value) {
  if (Offset >= DeviceReg::Size)
    return;

  // RIFF_PTR (8 bytes at offset 0x08, little-endian)
  if (Offset >= DeviceReg::RiffPtr && Offset < DeviceReg::Reserved) {
    unsigned ByteIdx = Offset - DeviceReg::RiffPtr;
    uint64_t Mask = ~(uint64_t(0xFF) << (ByteIdx * 8));
    RiffPtr = (RiffPtr & Mask) | (uint64_t(Value) << (ByteIdx * 8));
    return;
  }

  // DOORBELL (1 byte at offset 0x18) - any write triggers processing
  if (Offset == DeviceReg::Doorbell) {
    processRequest();
    return;
  }

  // STATUS (writing 0 clears response ready and deasserts IRQ)
  if (Offset == DeviceReg::Status) {
    if (Value == 0) {
      ResponseReady = false;
      if (Context *Ctx = Sys.getContext(0))
        Ctx->deassertIRQ();
    }
    return;
  }
}

//===----------------------------------------------------------------------===//
// Memory Access
//===----------------------------------------------------------------------===//

uint8_t Semihost::readMem(uint64_t Addr) { return Sys.read(Addr); }

void Semihost::writeMem(uint64_t Addr, uint8_t Val) { Sys.write(Addr, Val); }

void Semihost::readMemBlock(uint8_t *Dest, uint64_t Addr, size_t Size) {
  for (size_t I = 0; I < Size; ++I)
    Dest[I] = Sys.read(Addr + I);
}

void Semihost::writeMemBlock(uint64_t Addr, const uint8_t *Src, size_t Size) {
  for (size_t I = 0; I < Size; ++I)
    Sys.write(Addr + I, Src[I]);
}

//===----------------------------------------------------------------------===//
// Request Processing
//===----------------------------------------------------------------------===//

void Semihost::processRequest() {
  ResponseReady = false;

  // Read RIFF buffer from guest memory
  // First, read enough to get the RIFF header and determine size
  if (WorkBuffer.size() < sizeof(RiffHeader))
    WorkBuffer.resize(sizeof(RiffHeader));

  readMemBlock(WorkBuffer.data(), RiffPtr, sizeof(RiffHeader));

  // Parse the header to get total size
  auto *Hdr = reinterpret_cast<const RiffHeader *>(WorkBuffer.data());
  if (support::endian::read32le(&Hdr->RiffId) != FourCC::RIFF) {
    ResponseReady = true;
    return;
  }

  size_t TotalSize = 8 + support::endian::read32le(&Hdr->Size);
  if (TotalSize > WorkBuffer.size())
    WorkBuffer.resize(TotalSize);

  // Read the full RIFF message
  readMemBlock(WorkBuffer.data(), RiffPtr, TotalSize);

  // Parse the request
  Expected<ParsedRequest> MaybeReq =
      parseRequest(WorkBuffer.data(), TotalSize, Config);
  if (!MaybeReq) {
    consumeError(MaybeReq.takeError());
    ResponseReady = true;
    return;
  }

  ParsedRequest &Req = *MaybeReq;

  // Update config from CNFG chunk if present
  if (Req.HasCnfg)
    Config = Req.Config;

  // Dispatch the opcode
  if (Req.HasCall)
    dispatchOpcode(Req);

  // Write response back to guest memory
  writeMemBlock(RiffPtr, WorkBuffer.data(), TotalSize);

  ResponseReady = true;
}

void Semihost::dispatchOpcode(semihost::ParsedRequest &Req) {
  OpResult Result;

  switch (Req.Op) {
  case Opcode::Open: {
    if (Req.DataChunks.empty() || Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    StringRef Path = Req.getDataAsString(0);
    OpenMode Mode = static_cast<OpenMode>(Req.Parms[0]);

    // Policy check
    if (!ThePolicy->allowOpen(Path, Mode)) {
      Result = OpResult::error(EACCES);
      break;
    }

    // Resolve path through policy (may sandbox or reject)
    bool ForWrite = openModeIsWrite(Mode);
    Expected<std::string> ResolvedPath = ThePolicy->resolvePath(Path, ForWrite);
    if (!ResolvedPath) {
      consumeError(ResolvedPath.takeError());
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->open(*ResolvedPath, Mode);
    break;
  }

  case Opcode::Close: {
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);

    // Policy check
    if (!ThePolicy->allowClose(FD)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->close(FD);
    break;
  }

  case Opcode::Read: {
    if (Req.Parms.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);
    size_t Count = static_cast<size_t>(Req.Parms[1]);

    // Policy check
    if (!ThePolicy->allowRead(FD, Count)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->read(FD, Count);
    break;
  }

  case Opcode::Write: {
    if (Req.Parms.empty() || Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);
    ArrayRef<uint8_t> Data = Req.DataChunks[0].Data;

    // Policy check
    if (!ThePolicy->allowWrite(FD, Data)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->write(FD, Data);
    break;
  }

  case Opcode::WriteC: {
    char C;
    if (!Req.Parms.empty()) {
      C = static_cast<char>(Req.Parms[0]);
    } else if (!Req.DataChunks.empty() && !Req.DataChunks[0].Data.empty()) {
      C = static_cast<char>(Req.DataChunks[0].Data[0]);
    } else {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }

    // Policy check
    if (!ThePolicy->allowWriteChar(C)) {
      Result = OpResult::error(EACCES);
      break;
    }

    TheBackend->writeChar(C);
    Result = OpResult::success();
    break;
  }

  case Opcode::Write0: {
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    StringRef Str = Req.getDataAsString(0);

    // Policy check
    if (!ThePolicy->allowWriteString(Str)) {
      Result = OpResult::error(EACCES);
      break;
    }

    TheBackend->writeString(Str);
    Result = OpResult::success();
    break;
  }

  case Opcode::ReadC:
    // Policy check
    if (!ThePolicy->allowReadChar()) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = OpResult::success(TheBackend->readChar());
    break;

  case Opcode::Seek: {
    if (Req.Parms.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);
    int64_t Offset = static_cast<int64_t>(Req.Parms[1]);

    // Policy check
    if (!ThePolicy->allowSeek(FD, Offset)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->seek(FD, Offset);
    break;
  }

  case Opcode::FLen: {
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);

    // Policy check
    if (!ThePolicy->allowFileLength(FD)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->fileLength(FD);
    break;
  }

  case Opcode::Remove: {
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    StringRef Path = Req.getDataAsString(0);

    // Policy check
    if (!ThePolicy->allowRemove(Path)) {
      Result = OpResult::error(EACCES);
      break;
    }

    // Resolve path through policy
    Expected<std::string> ResolvedPath = ThePolicy->resolvePath(Path, true);
    if (!ResolvedPath) {
      consumeError(ResolvedPath.takeError());
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->remove(*ResolvedPath);
    break;
  }

  case Opcode::Rename: {
    if (Req.DataChunks.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    StringRef OldPath = Req.getDataAsString(0);
    StringRef NewPath = Req.getDataAsString(1);

    // Policy check
    if (!ThePolicy->allowRename(OldPath, NewPath)) {
      Result = OpResult::error(EACCES);
      break;
    }

    // Resolve both paths through policy
    Expected<std::string> ResolvedOld = ThePolicy->resolvePath(OldPath, true);
    if (!ResolvedOld) {
      consumeError(ResolvedOld.takeError());
      Result = OpResult::error(EACCES);
      break;
    }

    Expected<std::string> ResolvedNew = ThePolicy->resolvePath(NewPath, true);
    if (!ResolvedNew) {
      consumeError(ResolvedNew.takeError());
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->rename(*ResolvedOld, *ResolvedNew);
    break;
  }

  case Opcode::TmpNam: {
    int Id = Req.Parms.empty() ? 0 : static_cast<int>(Req.Parms[0]);

    // Policy check
    if (!ThePolicy->allowTmpnam(Id)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->tmpnam(Id);
    break;
  }

  case Opcode::IsError:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    // No policy check for IsError - it's a pure query
    Result = OpResult::success(
        TheBackend->isError(static_cast<int>(Req.Parms[0])) ? 1 : 0);
    break;

  case Opcode::IsTTY:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    // No policy check for IsTTY - it's a pure query
    Result =
        OpResult::success(TheBackend->isTTY(static_cast<int>(Req.Parms[0])) ? 1 : 0);
    break;

  case Opcode::Clock:
    // No policy check for Clock - it's a pure query
    Result = TheBackend->clock();
    break;

  case Opcode::Time:
    // No policy check for Time - it's a pure query
    Result = TheBackend->time();
    break;

  case Opcode::Elapsed:
    // No policy check for Elapsed - it's a pure query
    Result = TheBackend->elapsed();
    break;

  case Opcode::TickFreq:
    // No policy check for TickFreq - it's a pure query
    Result = TheBackend->tickFreq();
    break;

  case Opcode::System: {
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    StringRef Command = Req.getDataAsString(0);

    // Policy check - system() is dangerous
    if (!ThePolicy->allowSystem(Command)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->system(Command);
    break;
  }

  case Opcode::GetCmdLine:
    // Policy check
    if (!ThePolicy->allowGetCmdLine()) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->getCmdLine();
    break;

  case Opcode::HeapInfo:
    // Policy check
    if (!ThePolicy->allowHeapInfo()) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->heapInfo();
    break;

  case Opcode::Errno:
    // No policy check for Errno - it's a pure query
    Result = OpResult::success(TheBackend->getErrno());
    break;

  case Opcode::Exit:
  case Opcode::ExitExtended: {
    // No policy check for Exit - always allowed (guest wants to quit)
    unsigned Reason = Req.Parms.empty() ? 0 : static_cast<unsigned>(Req.Parms[0]);
    unsigned Subcode = Req.Parms.size() > 1 ? static_cast<unsigned>(Req.Parms[1]) : 0;
    TheBackend->exit(Reason, Subcode);
    Result = OpResult::success();
    break;
  }

  case Opcode::TimerConfig: {
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    unsigned RateHz = static_cast<unsigned>(Req.Parms[0]);

    // Policy check
    if (!ThePolicy->allowTimerConfig(RateHz)) {
      Result = OpResult::error(EACCES);
      break;
    }

    Result = TheBackend->timerConfig(RateHz);
    break;
  }
  }

  // Write the result
  if (auto Err = writeReturn(WorkBuffer.data(), Req, Result.Value, Result.Errno,
                             Result.Data)) {
    consumeError(std::move(Err));
  }
}
