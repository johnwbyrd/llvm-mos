//===-- llvm/Emulator/Semihost/PathValidator.h - Path Security --*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Path validation and sandboxing for secure semihosting.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_PATHVALIDATOR_H
#define LLVM_EMULATOR_SEMIHOST_PATHVALIDATOR_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
namespace emu {
namespace semihost {

/// Types of security violations that can occur.
enum class ViolationType {
  SandboxEscape,   ///< Attempted to escape sandbox via ".." or symlink
  NullByte,        ///< Path contains null byte
  NotAllowed,      ///< Path not in allowed list
  WriteProtected,  ///< Write attempted on read-only path
};

/// Callback for security violations.
using ViolationCallback =
    std::function<void(ViolationType Type, StringRef Path)>;

/// Configuration for path validation.
struct PathValidatorConfig {
  std::string SandboxDir;  ///< Root directory for sandboxing
  bool AllowSystem = false; ///< Allow shell command execution
  bool ReadOnly = false;    ///< Disallow all write operations

  /// Additional allowed paths beyond the sandbox.
  /// Each entry is (prefix, allow_write).
  std::vector<std::pair<std::string, bool>> AllowedPaths;

  /// Callback for security violations (optional).
  ViolationCallback OnViolation;
};

/// Path validator implementing sandbox security.
///
/// Validates paths to ensure:
/// - No null bytes in paths
/// - No escape via ".." after normalization
/// - No escape via symlinks
/// - Path is within sandbox or allowed paths
/// - Write access only where permitted
class PathValidator {
public:
  explicit PathValidator(PathValidatorConfig Config);

  /// Validate and resolve a path within the sandbox.
  /// @param Path The requested path.
  /// @param ForWrite True if the operation will write to the path.
  /// @return Resolved absolute path on success, Error if access denied.
  Expected<std::string> validate(StringRef Path, bool ForWrite) const;

  /// Check if system() calls are allowed.
  bool allowSystem() const { return Config_.AllowSystem; }

  /// Add an allowed path prefix.
  void addAllowedPath(StringRef Prefix, bool AllowWrite);

private:
  PathValidatorConfig Config_;

  /// Normalize a path (resolve ".", "..", remove duplicate slashes).
  std::string normalizePath(StringRef Path) const;

  /// Resolve symlinks and verify the path stays within sandbox.
  Expected<std::string> resolveAndCheck(StringRef Path, bool ForWrite) const;

  /// Check if a path is allowed (in sandbox or allowed paths).
  bool isAllowed(StringRef ResolvedPath, bool ForWrite) const;

  /// Report a security violation.
  void reportViolation(ViolationType Type, StringRef Path) const;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_PATHVALIDATOR_H
