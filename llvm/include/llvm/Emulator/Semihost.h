//===-- llvm/Emulator/Semihost.h - Semihosting Device -----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Semihost device for the emulator.
// Semihosting provides host I/O services to guest programs.
//
// Device register layout (32 bytes):
//   0x00-0x07: SIGNATURE - "ZBCSHOST" magic (read-only)
//   0x08-0x0F: RIFF_PTR  - Pointer to RIFF buffer in guest memory
//   0x10-0x17: Reserved
//   0x18:      DOORBELL  - Write triggers semihost call processing
//   0x19:      STATUS    - Bit 0: response ready
//   0x1A-0x1F: Reserved
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_H
#define LLVM_EMULATOR_SEMIHOST_H

#include "llvm/Emulator/Device.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Include ZBC headers for type definitions
#define ZBC_HOST
#include "llvm/Emulator/Semihost/zbc_semihost.h"

namespace llvm {
namespace emu {

class System;

/// Callback signature for exit events.
/// Called when the guest program requests an exit.
using ExitCallback = std::function<void(unsigned Reason, unsigned Subcode)>;

/// Semihosting device providing host I/O to guest programs.
///
/// This device implements the ZBC semihosting protocol, allowing guest
/// programs to perform file I/O, console I/O, and other host operations.
///
/// Usage:
/// \code
///   auto Semihost = Semihost::create(System, "/sandbox/dir");
///   System.addOwnedDevice(0xFCE0, 0xFCFF, std::move(Semihost));
/// \endcode
class Semihost : public Device {
public:
  /// Create a semihost device with sandboxed filesystem access.
  /// @param Sys System for memory access callbacks.
  /// @param SandboxDir Directory to sandbox file operations to.
  static std::unique_ptr<Semihost> create(System &Sys,
                                          const std::string &SandboxDir);

  /// Create a semihost device with UNRESTRICTED filesystem access.
  /// WARNING: Guest code can read/write/delete any file!
  static std::unique_ptr<Semihost> createInsecure(System &Sys);

  /// Create a semihost device with console-only access.
  /// Supports stdin/stdout/stderr and exit, but no filesystem access.
  /// File operations will return errors to the guest.
  static std::unique_ptr<Semihost> createConsoleOnly(System &Sys);

  ~Semihost() override;

  // Non-copyable
  Semihost(const Semihost &) = delete;
  Semihost &operator=(const Semihost &) = delete;

  //===--------------------------------------------------------------------===//
  // Device Interface
  //===--------------------------------------------------------------------===//

  uint8_t read(uint64_t Offset) override;
  void write(uint64_t Offset, uint8_t Value) override;

  //===--------------------------------------------------------------------===//
  // Configuration
  //===--------------------------------------------------------------------===//

  /// Set callback for exit events.
  void setExitCallback(ExitCallback CB) { OnExit = std::move(CB); }

  /// Add an additional allowed path (secure mode only).
  /// @param Prefix Path prefix to allow (e.g., "/usr/lib/").
  /// @param AllowWrite If true, allow write operations to this path.
  void addAllowedPath(const std::string &Prefix, bool AllowWrite = false);

  /// Check if response is ready (STATUS bit 0).
  bool isResponseReady() const { return ResponseReady; }

  /// Set STATUS to indicate timer tick.
  /// Called by System when timer fires to signal the IRQ handler.
  void setTimerTick() { ResponseReady = true; }

  /// Get the exit status if the guest has exited.
  /// Returns true if the guest has exited.
  bool hasExited() const { return Exited; }
  unsigned getExitReason() const { return ExitReason; }
  unsigned getExitSubcode() const { return ExitSubcode; }

private:
  Semihost(System &Sys, bool Secure, const std::string &SandboxDir);

  void initSecureBackend(const std::string &SandboxDir);
  void initInsecureBackend();
  void initConsoleBackend();
  void processRequest();

  // Memory callbacks for ZBC library
  static uint8_t memReadU8(uintptr_t Addr, void *Ctx);
  static void memWriteU8(uintptr_t Addr, uint8_t Val, void *Ctx);
  static void memReadBlock(void *Dest, uintptr_t Addr, size_t Size, void *Ctx);
  static void memWriteBlock(uintptr_t Addr, const void *Src, size_t Size,
                            void *Ctx);

  // Exit callback for secure backend
  static void onExitCallback(void *Ctx, unsigned Reason, unsigned Subcode);

  // Timer config callback for secure backend
  static void onTimerConfigCallback(void *Ctx, unsigned RateHz);

  System &Sys;
  bool Secure;

  // Device registers
  uint64_t RiffPtr = 0;
  bool ResponseReady = false;

  // Exit state
  bool Exited = false;
  unsigned ExitReason = 0;
  unsigned ExitSubcode = 0;
  ExitCallback OnExit;

  // ZBC state
  std::unique_ptr<zbc::zbc_host_state_t> HostState;
  std::unique_ptr<zbc::zbc_ansi_state_t> SecureState;
  std::unique_ptr<zbc::zbc_ansi_insecure_state_t> InsecureState;
  std::unique_ptr<zbc::zbc_ansi_console_state_t> ConsoleState;
  std::vector<uint8_t> WorkBuffer;

  // Signature bytes (per ZBC specification)
  static constexpr const char *Signature = "SEMIHOST";
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_H
