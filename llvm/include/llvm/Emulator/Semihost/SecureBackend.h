//===-- llvm/Emulator/Semihost/SecureBackend.h - Secure Backend -*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Sandboxed file backend with PathValidator for secure semihosting.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_SECUREBACKEND_H
#define LLVM_EMULATOR_SEMIHOST_SECUREBACKEND_H

#include "llvm/Emulator/Semihost/FileBackend.h"
#include "llvm/Emulator/Semihost/PathValidator.h"

namespace llvm {
namespace emu {
namespace semihost {

/// Sandboxed file backend with PathValidator.
///
/// All file operations are validated through the PathValidator to ensure
/// they stay within the sandbox directory.
class SecureBackend : public FileBackend {
public:
  SecureBackend(PathValidatorConfig ValidatorConfig, ExitCallback OnExit,
                TimerCallback OnTimer = nullptr);

  /// Add an allowed path prefix.
  void addAllowedPath(StringRef Prefix, bool AllowWrite);

  /// System command execution (blocked by default unless AllowSystem is true).
  OpResult system(StringRef Command) override;

protected:
  Expected<std::string> resolvePath(StringRef Path, bool ForWrite) override;

private:
  PathValidator Validator_;
};

/// Unrestricted file backend with no path validation.
///
/// WARNING: Guest code can read, write, and delete any file accessible
/// to the host process. Use only for trusted code.
class InsecureBackend : public FileBackend {
public:
  InsecureBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : FileBackend(std::move(OnExit), std::move(OnTimer)) {}

  /// System command execution (always allowed).
  OpResult system(StringRef Command) override;

protected:
  /// Just return the path as-is (no validation).
  Expected<std::string> resolvePath(StringRef Path, bool ForWrite) override;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_SECUREBACKEND_H
