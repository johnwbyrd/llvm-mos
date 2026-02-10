//===-- Semihost.cpp - Semihosting Device Implementation --------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost.h"
#include "llvm/Emulator/Semihost/PathValidator.h"
#include "llvm/Emulator/Semihost/SecureBackend.h"
#include "llvm/Emulator/System.h"
#include "llvm/Support/Error.h"

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
// Factory Methods
//===----------------------------------------------------------------------===//

std::unique_ptr<Semihost> Semihost::create(System &Sys,
                                           const std::string &SandboxDir) {
  // MOS 6502: 8-bit CPU, 16-bit pointers, little-endian
  PlatformConfig Config(2, 2, llvm::endianness::little);
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, nullptr, Config));

  PathValidatorConfig ValidatorConfig;
  ValidatorConfig.SandboxDir = SandboxDir;

  auto *Back = new SecureBackend(std::move(ValidatorConfig),
                                 Dev->makeExitCallback(),
                                 makeTimerCallback(Sys));
  Dev->TheBackend.reset(Back);
  Dev->SecureBack = Back;

  return Dev;
}

std::unique_ptr<Semihost> Semihost::createInsecure(System &Sys) {
  PlatformConfig Config(2, 2, llvm::endianness::little);
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, nullptr, Config));

  Dev->TheBackend = std::make_unique<InsecureBackend>(Dev->makeExitCallback(),
                                                      makeTimerCallback(Sys));
  return Dev;
}

std::unique_ptr<Semihost> Semihost::createConsoleOnly(System &Sys) {
  PlatformConfig Config(2, 2, llvm::endianness::little);
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, nullptr, Config));

  Dev->TheBackend = std::make_unique<ConsoleBackend>(Dev->makeExitCallback(),
                                                     makeTimerCallback(Sys));
  return Dev;
}

//===----------------------------------------------------------------------===//
// Constructor/Destructor
//===----------------------------------------------------------------------===//

Semihost::Semihost(System &Sys, std::unique_ptr<semihost::Backend> Back,
                   semihost::PlatformConfig Cfg)
    : Sys(Sys), TheBackend(std::move(Back)), Config(Cfg),
      WorkBuffer(WorkBufferSize) {}

Semihost::~Semihost() = default;

//===----------------------------------------------------------------------===//
// Configuration
//===----------------------------------------------------------------------===//

void Semihost::setExitCallback(semihost::ExitCallback CB) {
  OnExit = std::move(CB);
}

void Semihost::addAllowedPath(const std::string &Prefix, bool AllowWrite) {
  // SecureBackend stores a pointer to itself that we can check
  if (SecureBack)
    SecureBack->addAllowedPath(Prefix, AllowWrite);
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
    Result = TheBackend->open(Req.getDataAsString(0),
                              static_cast<OpenMode>(Req.Parms[0]));
    break;
  }

  case Opcode::Close:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->close(static_cast<int>(Req.Parms[0]));
    break;

  case Opcode::Read: {
    if (Req.Parms.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);
    size_t Count = static_cast<size_t>(Req.Parms[1]);
    Result = TheBackend->read(FD, Count);
    break;
  }

  case Opcode::Write: {
    if (Req.Parms.empty() || Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    int FD = static_cast<int>(Req.Parms[0]);
    Result = TheBackend->write(FD, Req.DataChunks[0].Data);
    break;
  }

  case Opcode::WriteC:
    // WriteC can receive the character via PARM or DATA chunk
    if (!Req.Parms.empty()) {
      TheBackend->writeChar(static_cast<char>(Req.Parms[0]));
    } else if (!Req.DataChunks.empty() && !Req.DataChunks[0].Data.empty()) {
      TheBackend->writeChar(static_cast<char>(Req.DataChunks[0].Data[0]));
    } else {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = OpResult::success();
    break;

  case Opcode::Write0:
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    TheBackend->writeString(Req.getDataAsString(0));
    Result = OpResult::success();
    break;

  case Opcode::ReadC:
    Result = OpResult::success(TheBackend->readChar());
    break;

  case Opcode::Seek:
    if (Req.Parms.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->seek(static_cast<int>(Req.Parms[0]),
                              static_cast<int64_t>(Req.Parms[1]));
    break;

  case Opcode::FLen:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->fileLength(static_cast<int>(Req.Parms[0]));
    break;

  case Opcode::Remove:
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->remove(Req.getDataAsString(0));
    break;

  case Opcode::Rename:
    if (Req.DataChunks.size() < 2) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->rename(Req.getDataAsString(0), Req.getDataAsString(1));
    break;

  case Opcode::TmpNam:
    Result = TheBackend->tmpnam(Req.Parms.empty() ? 0 : static_cast<int>(Req.Parms[0]));
    break;

  case Opcode::IsError:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = OpResult::success(
        TheBackend->isError(static_cast<int>(Req.Parms[0])) ? 1 : 0);
    break;

  case Opcode::IsTTY:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result =
        OpResult::success(TheBackend->isTTY(static_cast<int>(Req.Parms[0])) ? 1 : 0);
    break;

  case Opcode::Clock:
    Result = TheBackend->clock();
    break;

  case Opcode::Time:
    Result = TheBackend->time();
    break;

  case Opcode::Elapsed:
    Result = TheBackend->elapsed();
    break;

  case Opcode::TickFreq:
    Result = TheBackend->tickFreq();
    break;

  case Opcode::System:
    if (Req.DataChunks.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->system(Req.getDataAsString(0));
    break;

  case Opcode::GetCmdLine:
    Result = TheBackend->getCmdLine();
    break;

  case Opcode::HeapInfo:
    Result = TheBackend->heapInfo();
    break;

  case Opcode::Errno:
    Result = OpResult::success(TheBackend->getErrno());
    break;

  case Opcode::Exit:
  case Opcode::ExitExtended: {
    unsigned Reason = Req.Parms.empty() ? 0 : static_cast<unsigned>(Req.Parms[0]);
    unsigned Subcode = Req.Parms.size() > 1 ? static_cast<unsigned>(Req.Parms[1]) : 0;
    TheBackend->exit(Reason, Subcode);
    Result = OpResult::success();
    break;
  }

  case Opcode::TimerConfig:
    if (Req.Parms.empty()) {
      consumeError(writeError(WorkBuffer.data(), Req, ProtoError::InvalidParams));
      return;
    }
    Result = TheBackend->timerConfig(static_cast<unsigned>(Req.Parms[0]));
    break;
  }

  // Write the result
  if (auto Err = writeReturn(WorkBuffer.data(), Req, Result.Value, Result.Errno,
                             Result.Data)) {
    consumeError(std::move(Err));
  }
}
