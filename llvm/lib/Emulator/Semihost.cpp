//===-- Semihost.cpp - Semihosting Device Implementation --------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost.h"
#include "llvm/Emulator/System.h"

// Include ZBC headers
#define ZBC_HOST
#include "llvm/Emulator/Semihost/zbc_semihost.h"

using namespace llvm;
using namespace llvm::emu;
using namespace llvm::zbc;

// Register offsets within the 32-byte device region
namespace {
constexpr uint64_t REG_RIFF_PTR = 0x08;  // 8 bytes (little-endian pointer)
constexpr uint64_t REG_RESERVED1 = 0x10; // 8 bytes
constexpr uint64_t REG_DOORBELL = 0x18;  // 1 byte (write triggers call)
constexpr uint64_t REG_STATUS = 0x19;    // 1 byte (bit 0 = response ready)
constexpr uint64_t DEVICE_SIZE = 0x20;   // 32 bytes total

constexpr size_t WORK_BUFFER_SIZE = 1024;
} // namespace

// Backend mode for constructor
enum class BackendMode { Secure, Insecure, ConsoleOnly };

std::unique_ptr<Semihost> Semihost::create(System &Sys,
                                           const std::string &SandboxDir) {
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, true, SandboxDir));
  Dev->initSecureBackend(SandboxDir);
  return Dev;
}

std::unique_ptr<Semihost> Semihost::createInsecure(System &Sys) {
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, false, ""));
  Dev->initInsecureBackend();
  return Dev;
}

std::unique_ptr<Semihost> Semihost::createConsoleOnly(System &Sys) {
  auto Dev = std::unique_ptr<Semihost>(new Semihost(Sys, false, ""));
  Dev->initConsoleBackend();
  return Dev;
}

Semihost::Semihost(System &Sys, bool Secure, const std::string &SandboxDir)
    : Sys(Sys), Secure(Secure), WorkBuffer(WORK_BUFFER_SIZE) {
  HostState = std::make_unique<zbc_host_state_t>();
  // Backend initialization is done by the factory methods
  (void)SandboxDir; // Used by initSecureBackend called from factory
}

Semihost::~Semihost() {
  // Clean up backend state
  if (SecureState) {
    zbc_ansi_cleanup(SecureState.get());
  }
  if (InsecureState) {
    zbc_ansi_insecure_cleanup(InsecureState.get());
  }
  if (ConsoleState) {
    zbc_ansi_console_cleanup(ConsoleState.get());
  }
}

void Semihost::initSecureBackend(const std::string &SandboxDir) {
  SecureState = std::make_unique<zbc_ansi_state_t>();
  zbc_ansi_init(SecureState.get(), SandboxDir.c_str());

  // Set up exit and timer config callbacks
  zbc_ansi_set_callbacks(SecureState.get(), nullptr, onExitCallback,
                         onTimerConfigCallback, this);

  // Initialize host state
  zbc_host_mem_ops_t MemOps = {memReadU8, memWriteU8, memReadBlock,
                               memWriteBlock};
  zbc_host_init(HostState.get(), &MemOps, this, zbc_backend_ansi(),
                SecureState.get(), WorkBuffer.data(), WorkBuffer.size());
}

void Semihost::initInsecureBackend() {
  InsecureState = std::make_unique<zbc_ansi_insecure_state_t>();
  zbc_ansi_insecure_init(InsecureState.get());

  // Initialize host state
  zbc_host_mem_ops_t MemOps = {memReadU8, memWriteU8, memReadBlock,
                               memWriteBlock};
  zbc_host_init(HostState.get(), &MemOps, this, zbc_backend_ansi_insecure(),
                InsecureState.get(), WorkBuffer.data(), WorkBuffer.size());
}

void Semihost::initConsoleBackend() {
  ConsoleState = std::make_unique<zbc_ansi_console_state_t>();
  zbc_ansi_console_init(ConsoleState.get());

  // Set up exit callback
  zbc_ansi_console_set_exit_callback(ConsoleState.get(), onExitCallback, this);

  // Set up timer config callback
  zbc_ansi_console_set_timer_callback(ConsoleState.get(), onTimerConfigCallback,
                                      this);

  // Initialize host state
  zbc_host_mem_ops_t MemOps = {memReadU8, memWriteU8, memReadBlock,
                               memWriteBlock};
  zbc_host_init(HostState.get(), &MemOps, this, zbc_backend_console(),
                ConsoleState.get(), WorkBuffer.data(), WorkBuffer.size());
}

uint8_t Semihost::read(uint64_t Offset) {
  if (Offset >= DEVICE_SIZE)
    return 0xFF;

  // Signature (8 bytes at offset 0)
  if (Offset < 8) {
    return static_cast<uint8_t>(Signature[Offset]);
  }

  // RIFF_PTR (8 bytes at offset 0x08, little-endian)
  if (Offset >= REG_RIFF_PTR && Offset < REG_RESERVED1) {
    unsigned ByteIdx = Offset - REG_RIFF_PTR;
    return static_cast<uint8_t>((RiffPtr >> (ByteIdx * 8)) & 0xFF);
  }

  // STATUS (1 byte at offset 0x19)
  if (Offset == REG_STATUS) {
    return ResponseReady ? 0x01 : 0x00;
  }

  // Reserved/unmapped reads return 0
  return 0x00;
}

void Semihost::write(uint64_t Offset, uint8_t Value) {
  if (Offset >= DEVICE_SIZE)
    return;

  // RIFF_PTR (8 bytes at offset 0x08, little-endian)
  if (Offset >= REG_RIFF_PTR && Offset < REG_RESERVED1) {
    unsigned ByteIdx = Offset - REG_RIFF_PTR;
    uint64_t Mask = ~(static_cast<uint64_t>(0xFF) << (ByteIdx * 8));
    RiffPtr = (RiffPtr & Mask) | (static_cast<uint64_t>(Value) << (ByteIdx * 8));
    return;
  }

  // DOORBELL (1 byte at offset 0x18) - any write triggers processing
  if (Offset == REG_DOORBELL) {
    processRequest();
    return;
  }

  // STATUS (writing 0 clears response ready and deasserts IRQ)
  if (Offset == REG_STATUS) {
    if (Value == 0) {
      ResponseReady = false;
      // Deassert IRQ when guest acknowledges by writing 0 to STATUS
      // This matches MAME's level-triggered IRQ behavior
      if (Context *Ctx = Sys.getContext(0))
        Ctx->deassertIRQ();
    }
    return;
  }

  // Writes to signature and reserved areas are ignored
}

void Semihost::processRequest() {
  // Clear response ready before processing
  ResponseReady = false;

  // Process the semihost request
  int Result = zbc_host_process(HostState.get(), static_cast<uintptr_t>(RiffPtr));

  // Set response ready (even on error, so guest knows to check)
  ResponseReady = true;

  (void)Result; // Result is written to RIFF buffer
}

void Semihost::addAllowedPath(const std::string &Prefix, bool AllowWrite) {
  if (Secure && SecureState) {
    zbc_ansi_add_path(SecureState.get(), Prefix.c_str(), AllowWrite ? 1 : 0);
  }
}

//===----------------------------------------------------------------------===//
// Memory Callbacks
//===----------------------------------------------------------------------===//

uint8_t Semihost::memReadU8(uintptr_t Addr, void *Ctx) {
  auto *Self = static_cast<Semihost *>(Ctx);
  return Self->Sys.read(Addr);
}

void Semihost::memWriteU8(uintptr_t Addr, uint8_t Val, void *Ctx) {
  auto *Self = static_cast<Semihost *>(Ctx);
  Self->Sys.write(Addr, Val);
}

void Semihost::memReadBlock(void *Dest, uintptr_t Addr, size_t Size,
                            void *Ctx) {
  auto *Self = static_cast<Semihost *>(Ctx);
  auto *DestBytes = static_cast<uint8_t *>(Dest);
  for (size_t I = 0; I < Size; ++I) {
    DestBytes[I] = Self->Sys.read(Addr + I);
  }
}

void Semihost::memWriteBlock(uintptr_t Addr, const void *Src, size_t Size,
                             void *Ctx) {
  auto *Self = static_cast<Semihost *>(Ctx);
  const auto *SrcBytes = static_cast<const uint8_t *>(Src);
  for (size_t I = 0; I < Size; ++I) {
    Self->Sys.write(Addr + I, SrcBytes[I]);
  }
}

//===----------------------------------------------------------------------===//
// Exit Callback
//===----------------------------------------------------------------------===//

void Semihost::onExitCallback(void *Ctx, unsigned Reason, unsigned Subcode) {
  auto *Self = static_cast<Semihost *>(Ctx);
  Self->Exited = true;
  Self->ExitReason = Reason;
  Self->ExitSubcode = Subcode;

  if (Self->OnExit) {
    Self->OnExit(Reason, Subcode);
  }
}

//===----------------------------------------------------------------------===//
// Timer Config Callback
//===----------------------------------------------------------------------===//

void Semihost::onTimerConfigCallback(void *Ctx, unsigned RateHz) {
  auto *Self = static_cast<Semihost *>(Ctx);
  // Register ourselves with System so it can call setTimerTick() on timer fire
  Self->Sys.setSemihostDevice(Self);
  // Configure the timer in System
  Self->Sys.configureTimer(RateHz);
}
