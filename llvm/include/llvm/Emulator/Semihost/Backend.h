//===-- llvm/Emulator/Semihost/Backend.h - Backend Interface ----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Abstract backend interface for semihosting operations.
/// The base class is secure by default - all operations return errors.
/// Subclasses enable specific capabilities.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_BACKEND_H
#define LLVM_EMULATOR_SEMIHOST_BACKEND_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Emulator/Semihost/Protocol.h"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <functional>

namespace llvm {
namespace emu {
namespace semihost {

//===----------------------------------------------------------------------===//
// OpResult - Syscall operation result
//===----------------------------------------------------------------------===//

/// Result of a semihosting operation.
/// Carries return value, errno, and optional output data.
struct OpResult {
  intmax_t Value = 0;  ///< Return value (meaning varies by operation)
  int Errno = 0;       ///< errno value (0 for success)
  SmallVector<uint8_t, 256> Data;  ///< Optional output data

  /// Create a successful result.
  static OpResult success(intmax_t V = 0) { return {V, 0, {}}; }

  /// Create an error result.
  static OpResult error(int E) { return {-1, E, {}}; }

  /// Check if this is an error.
  bool isError() const { return Value < 0 && Errno != 0; }
};

//===----------------------------------------------------------------------===//
// Callbacks
//===----------------------------------------------------------------------===//

/// Callback for guest exit events.
using ExitCallback = std::function<void(unsigned Reason, unsigned Subcode)>;

/// Callback for timer configuration.
using TimerCallback = std::function<void(unsigned RateHz)>;

//===----------------------------------------------------------------------===//
// Backend - Abstract base class (secure by default)
//===----------------------------------------------------------------------===//

/// Abstract base class for semihosting backends.
///
/// This base class is SECURE BY DEFAULT - all operations return errors.
/// Subclasses override methods to enable specific capabilities:
///
///   Backend (all errors)
///     └── ConsoleBackend (adds stdin/stdout/stderr)
///             └── FileBackend (adds file operations)
///                     ├── SecureBackend (sandboxed)
///                     └── InsecureBackend (unrestricted)
///
class Backend {
public:
  virtual ~Backend() = default;

  //===--------------------------------------------------------------------===//
  // File Operations (default: return errors)
  //===--------------------------------------------------------------------===//

  /// Open a file.
  /// @param Path File path (NOT null-terminated; use PathLen).
  /// @param PathLen Length of path in bytes.
  /// @param Mode Open mode (OpenMode enum value).
  /// @return FD on success, -1 with errno on error.
  virtual OpResult open(StringRef Path, OpenMode Mode) {
    return OpResult::error(ENOSYS);
  }

  /// Close a file descriptor.
  virtual OpResult close(int FD) { return OpResult::error(EBADF); }

  /// Read from a file descriptor.
  /// @param FD File descriptor.
  /// @param Count Maximum bytes to read.
  /// @return Bytes NOT read (0 = success), -1 with errno on error.
  ///         Output data in OpResult::Data.
  virtual OpResult read(int FD, size_t Count) {
    return OpResult::error(EBADF);
  }

  /// Write to a file descriptor.
  /// @param FD File descriptor.
  /// @param Data Data to write.
  /// @return Bytes NOT written (0 = success), -1 with errno on error.
  virtual OpResult write(int FD, ArrayRef<uint8_t> Data) {
    return OpResult::error(EBADF);
  }

  /// Seek to an absolute position.
  virtual OpResult seek(int FD, int64_t Pos) { return OpResult::error(EBADF); }

  /// Get file length.
  virtual OpResult fileLength(int FD) { return OpResult::error(EBADF); }

  /// Remove (delete) a file.
  virtual OpResult remove(StringRef Path) { return OpResult::error(ENOSYS); }

  /// Rename a file.
  virtual OpResult rename(StringRef OldPath, StringRef NewPath) {
    return OpResult::error(ENOSYS);
  }

  /// Generate a temporary filename.
  virtual OpResult tmpnam(int Id) { return OpResult::error(ENOSYS); }

  //===--------------------------------------------------------------------===//
  // Console Operations (default: no-op or return error)
  //===--------------------------------------------------------------------===//

  /// Write a single character to console.
  virtual void writeChar(char C) {}

  /// Write a string to console.
  virtual void writeString(StringRef Str) {}

  /// Read a character from console (blocking).
  /// @return Character read, or -1 if unavailable.
  virtual int readChar() { return -1; }

  //===--------------------------------------------------------------------===//
  // Query Operations
  //===--------------------------------------------------------------------===//

  /// Check if a status value indicates an error.
  virtual bool isError(int Status) { return Status < 0; }

  /// Check if a file descriptor is a TTY.
  virtual bool isTTY(int FD) { return false; }

  //===--------------------------------------------------------------------===//
  // Time Operations (default: return error)
  //===--------------------------------------------------------------------===//

  /// Get centiseconds since program start.
  virtual OpResult clock() { return OpResult::error(ENOSYS); }

  /// Get seconds since Unix epoch.
  virtual OpResult time() { return OpResult::error(ENOSYS); }

  /// Get 64-bit tick count.
  /// @return Low 32 bits in Value, high 32 bits in Data[0..3] (LE).
  virtual OpResult elapsed() { return OpResult::error(ENOSYS); }

  /// Get ticks per second.
  virtual OpResult tickFreq() { return OpResult::error(ENOSYS); }

  //===--------------------------------------------------------------------===//
  // System Operations
  //===--------------------------------------------------------------------===//

  /// Execute a shell command.
  virtual OpResult system(StringRef Command) { return OpResult::error(ENOSYS); }

  /// Get command line arguments.
  virtual OpResult getCmdLine() { return OpResult::error(ENOSYS); }

  /// Get heap/stack info.
  /// Returns 4 pointer values in Data: heap_base, heap_limit,
  /// stack_base, stack_limit.
  virtual OpResult heapInfo() { return OpResult::error(ENOSYS); }

  /// Get last errno value.
  virtual int getErrno() { return LastErrno; }

  /// Exit the program.
  /// This must be implemented - there's no sensible default.
  virtual void exit(unsigned Reason, unsigned Subcode) = 0;

  /// Configure periodic timer.
  /// @param RateHz Timer frequency in Hz (0 = disable).
  virtual OpResult timerConfig(unsigned RateHz) {
    return OpResult::error(ENOSYS);
  }

protected:
  int LastErrno = 0;  ///< Most recent errno for getErrno()
};

//===----------------------------------------------------------------------===//
// ConsoleBackend - Adds stdin/stdout/stderr support
//===----------------------------------------------------------------------===//

/// Backend that supports console I/O (stdin/stdout/stderr).
/// File operations still return errors.
class ConsoleBackend : public Backend {
public:
  /// Construct with exit and timer callbacks.
  ConsoleBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : OnExit_(std::move(OnExit)), OnTimer_(std::move(OnTimer)) {}

  // Console operations
  void writeChar(char C) override;
  void writeString(StringRef Str) override;
  int readChar() override;

  // Read/write work for FD 0/1/2
  OpResult read(int FD, size_t Count) override;
  OpResult write(int FD, ArrayRef<uint8_t> Data) override;

  // FD 0/1/2 are TTYs
  bool isTTY(int FD) override { return FD >= 0 && FD <= 2; }

  // Time operations
  OpResult clock() override;
  OpResult time() override;

  // Exit handling
  void exit(unsigned Reason, unsigned Subcode) override;

  // Timer configuration
  OpResult timerConfig(unsigned RateHz) override;

private:
  ExitCallback OnExit_;
  TimerCallback OnTimer_;
  std::chrono::steady_clock::time_point StartTime_ =
      std::chrono::steady_clock::now();
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_BACKEND_H
