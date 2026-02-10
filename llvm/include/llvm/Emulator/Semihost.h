//===-- llvm/Emulator/Semihost.h - Semihosting Device -----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Bridges guest code to host I/O services.
///
/// Without I/O, an emulated CPU is completely isolated. It can execute
/// instructions and modify its own memory, but it cannot print output, read
/// files, or even report whether it succeeded or failed. Semihosting solves
/// this by providing a memory-mapped device that guest code can write to,
/// triggering the host to perform I/O operations on its behalf.
///
/// This implementation uses the ZBC protocol: guest code builds a RIFF-encoded
/// request in memory, writes the buffer address to RIFF_PTR, then writes to
/// DOORBELL to trigger processing. The host reads the request, performs the
/// operation, writes the response back, and sets STATUS to signal completion.
///
/// Register layout (32 bytes at mapped address):
///   0x00-0x07: SIGNATURE  "SEMIHOST" magic (read-only)
///   0x08-0x0F: RIFF_PTR   Pointer to request/response buffer
///   0x18:      DOORBELL   Write triggers request processing
///   0x19:      STATUS     Bit 0 set when response ready
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_H
#define LLVM_EMULATOR_SEMIHOST_H

#include "llvm/Emulator/Device.h"
#include "llvm/Emulator/Semihost/Backend.h"
#include "llvm/Emulator/Semihost/Policy.h"
#include "llvm/Emulator/Semihost/RiffCodec.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace llvm {
namespace emu {

class System;

/// Semihosting device providing host I/O to guest programs.
///
/// This device implements the ZBC semihosting protocol, allowing guest
/// programs to perform file I/O, console I/O, and other host operations.
///
/// Usage:
/// \code
///   auto Semihost = Semihost::create(System, Config, Policy);
///   System.addDevice(0xFCE0, 0xFCFF, std::move(Semihost));
/// \endcode
class Semihost : public Device {
public:
  /// Create a semihost device with the given security policy.
  /// @param Sys System for memory access callbacks.
  /// @param Config Platform configuration for RIFF encoding.
  /// @param ThePolicy Security policy controlling what operations are allowed.
  static std::unique_ptr<Semihost>
  create(System &Sys, semihost::PlatformConfig Config,
         std::unique_ptr<semihost::Policy> ThePolicy);

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
  void setExitCallback(semihost::ExitCallback CB);

  /// Add an additional allowed path (SandboxedPolicy only).
  /// @param Prefix Path prefix to allow (e.g., "/usr/lib/").
  /// @param AllowWrite If true, allow write operations to this path.
  /// @note Only works if the Policy is a SandboxedPolicy.
  void addAllowedPath(const std::string &Prefix, bool AllowWrite = false);

  /// Check if response is ready (STATUS bit 0).
  bool isResponseReady() const { return ResponseReady; }

  /// Set STATUS to indicate timer tick.
  /// Called by System when timer fires to signal the IRQ handler.
  void setTimerTick() { ResponseReady = true; }

  /// Get the exit status if the guest has exited.
  bool hasExited() const { return Exited; }
  unsigned getExitReason() const { return ExitReason; }
  unsigned getExitSubcode() const { return ExitSubcode; }

private:
  Semihost(System &Sys, std::unique_ptr<semihost::Backend> Back,
           std::unique_ptr<semihost::Policy> Pol,
           semihost::PlatformConfig Config);

  /// Create exit callback for backend initialization.
  semihost::ExitCallback makeExitCallback();

  /// Create timer callback for backend initialization.
  static semihost::TimerCallback makeTimerCallback(System &Sys);

  void processRequest();
  void dispatchOpcode(semihost::ParsedRequest &Req);

  // Read/write guest memory
  uint8_t readMem(uint64_t Addr);
  void writeMem(uint64_t Addr, uint8_t Val);
  void readMemBlock(uint8_t *Dest, uint64_t Addr, size_t Size);
  void writeMemBlock(uint64_t Addr, const uint8_t *Src, size_t Size);

  System &Sys;
  std::unique_ptr<semihost::Backend> TheBackend;
  std::unique_ptr<semihost::Policy> ThePolicy;
  semihost::PlatformConfig Config;

  // Device registers
  uint64_t RiffPtr = 0;
  bool ResponseReady = false;

  // Exit state
  bool Exited = false;
  unsigned ExitReason = 0;
  unsigned ExitSubcode = 0;
  semihost::ExitCallback OnExit;

  // Work buffer for RIFF processing
  std::vector<uint8_t> WorkBuffer;

  // Signature bytes
  static constexpr const char *Signature = "SEMIHOST";
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_H
