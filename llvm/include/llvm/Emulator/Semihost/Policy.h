//===-- llvm/Emulator/Semihost/Policy.h - Security Policy ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Security policy interface for semihosting operations.
///
/// The Policy class defines per-operation allow/deny methods. Subclass to
/// implement custom security policies or integrate with external security
/// systems.
///
/// Design follows Linux Security Modules pattern: hooks at decision points,
/// not wrappers. Semihost::dispatchOpcode() checks Policy before each
/// backend call.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_POLICY_H
#define LLVM_EMULATOR_SEMIHOST_POLICY_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Emulator/Semihost/PathValidator.h"
#include "llvm/Emulator/Semihost/Protocol.h"
#include "llvm/Support/Error.h"
#include <memory>
#include <string>

namespace llvm {
namespace emu {
namespace semihost {

/// Security policy for semihosting operations.
///
/// Base class is SECURE BY DEFAULT - all operations are denied.
/// Subclass and override to allow specific operations.
///
/// Example policies:
/// - ConsoleOnlyPolicy: allows stdin/stdout/stderr only
/// - SandboxedPolicy: allows filesystem within a directory
/// - UnrestrictedPolicy: allows everything (dangerous)
///
class Policy {
public:
  virtual ~Policy() = default;

  //===--------------------------------------------------------------------===//
  // File Operations
  //===--------------------------------------------------------------------===//

  /// Check if opening a file is allowed.
  /// @param Path File path to open.
  /// @param Mode Open mode (read, write, append, etc.).
  /// @return true to allow, false to deny.
  virtual bool allowOpen(StringRef Path, OpenMode Mode) { return false; }

  /// Check if closing a file descriptor is allowed.
  /// Generally safe to allow - denying could leak resources.
  virtual bool allowClose(int FD) { return true; }

  /// Check if reading from a file descriptor is allowed.
  virtual bool allowRead(int FD, size_t Count) { return false; }

  /// Check if writing to a file descriptor is allowed.
  virtual bool allowWrite(int FD, ArrayRef<uint8_t> Data) { return false; }

  /// Check if seeking in a file is allowed.
  virtual bool allowSeek(int FD, int64_t Offset) { return false; }

  /// Check if getting file length is allowed.
  virtual bool allowFileLength(int FD) { return false; }

  /// Check if removing (deleting) a file is allowed.
  virtual bool allowRemove(StringRef Path) { return false; }

  /// Check if renaming a file is allowed.
  virtual bool allowRename(StringRef OldPath, StringRef NewPath) {
    return false;
  }

  /// Check if creating a temporary file is allowed.
  virtual bool allowTmpnam(int Id) { return false; }

  //===--------------------------------------------------------------------===//
  // Console Operations
  //===--------------------------------------------------------------------===//

  /// Check if reading a character from stdin is allowed.
  virtual bool allowReadChar() { return false; }

  /// Check if writing a character to stdout is allowed.
  virtual bool allowWriteChar(char C) { return false; }

  /// Check if writing a string to stdout is allowed.
  virtual bool allowWriteString(StringRef Str) { return false; }

  //===--------------------------------------------------------------------===//
  // System Operations
  //===--------------------------------------------------------------------===//

  /// Check if executing a shell command is allowed.
  /// This is DANGEROUS - allows arbitrary code execution on host.
  virtual bool allowSystem(StringRef Command) { return false; }

  /// Check if getting command line arguments is allowed.
  virtual bool allowGetCmdLine() { return false; }

  /// Check if getting heap/stack info is allowed.
  virtual bool allowHeapInfo() { return false; }

  /// Check if configuring the timer IRQ is allowed.
  virtual bool allowTimerConfig(unsigned RateHz) { return false; }

  //===--------------------------------------------------------------------===//
  // Path Resolution (for sandboxing)
  //===--------------------------------------------------------------------===//

  /// Resolve and validate a file path.
  ///
  /// This is called for path-based operations (open, remove, rename) after
  /// the allow check passes. It can:
  /// - Transform the path (e.g., make relative to sandbox)
  /// - Reject paths that escape the sandbox
  /// - Enforce read-only for certain paths
  ///
  /// @param Path Original path from guest.
  /// @param ForWrite true if the operation will modify the file.
  /// @return Resolved path on success, Error on rejection.
  virtual Expected<std::string> resolvePath(StringRef Path, bool ForWrite) {
    return std::string(Path);
  }

  //===--------------------------------------------------------------------===//
  // Configuration (optional, not all policies support this)
  //===--------------------------------------------------------------------===//

  /// Add an additional allowed path prefix.
  /// Only meaningful for policies that support path allowlists (e.g., SandboxedPolicy).
  /// Default implementation does nothing.
  virtual void addAllowedPath(StringRef Prefix, bool AllowWrite) {}
};

//===----------------------------------------------------------------------===//
// Preset Policies
//===----------------------------------------------------------------------===//

/// Policy that allows console I/O only (stdin/stdout/stderr).
/// No filesystem access, no system commands.
class ConsoleOnlyPolicy : public Policy {
public:
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(StringRef) override { return true; }

  // Allow read/write on FD 0/1/2 only
  bool allowRead(int FD, size_t) override { return FD >= 0 && FD <= 2; }
  bool allowWrite(int FD, ArrayRef<uint8_t>) override {
    return FD >= 1 && FD <= 2;
  }
};

/// Policy that allows everything. DANGEROUS - use only for trusted code.
class UnrestrictedPolicy : public Policy {
public:
  bool allowOpen(StringRef, OpenMode) override { return true; }
  bool allowRead(int, size_t) override { return true; }
  bool allowWrite(int, ArrayRef<uint8_t>) override { return true; }
  bool allowSeek(int, int64_t) override { return true; }
  bool allowFileLength(int) override { return true; }
  bool allowRemove(StringRef) override { return true; }
  bool allowRename(StringRef, StringRef) override { return true; }
  bool allowTmpnam(int) override { return true; }
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(StringRef) override { return true; }
  bool allowSystem(StringRef) override { return true; }
  bool allowGetCmdLine() override { return true; }
  bool allowHeapInfo() override { return true; }
  bool allowTimerConfig(unsigned) override { return true; }
};

/// Policy that sandboxes filesystem access to a directory.
/// Uses PathValidator for path resolution and security checks.
class SandboxedPolicy : public Policy {
public:
  /// Create a sandboxed policy.
  /// @param SandboxDir Directory to sandbox file operations to.
  explicit SandboxedPolicy(StringRef SandboxDir);

  // File operations allowed (actual path check in resolvePath)
  bool allowOpen(StringRef, OpenMode) override { return true; }
  bool allowRead(int, size_t) override { return true; }
  bool allowWrite(int, ArrayRef<uint8_t>) override { return true; }
  bool allowSeek(int, int64_t) override { return true; }
  bool allowFileLength(int) override { return true; }
  bool allowRemove(StringRef) override { return true; }
  bool allowRename(StringRef, StringRef) override { return true; }
  bool allowTmpnam(int) override { return true; }

  // Console allowed
  bool allowReadChar() override { return true; }
  bool allowWriteChar(char) override { return true; }
  bool allowWriteString(StringRef) override { return true; }

  // Timer allowed
  bool allowTimerConfig(unsigned) override { return true; }

  // Path resolution uses PathValidator for sandboxing
  Expected<std::string> resolvePath(StringRef Path, bool ForWrite) override;

  /// Add an additional allowed path.
  void addAllowedPath(StringRef Prefix, bool AllowWrite) override;

private:
  std::unique_ptr<PathValidator> Validator;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_POLICY_H
