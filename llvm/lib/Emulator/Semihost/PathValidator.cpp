//===-- PathValidator.cpp - Path Security Implementation -------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/PathValidator.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <algorithm>

using namespace llvm;
using namespace llvm::emu::semihost;

PathValidator::PathValidator(PathValidatorConfig Config)
    : Config_(std::move(Config)) {
  // Normalize the sandbox directory - MUST resolve to real path
  if (!Config_.SandboxDir.empty()) {
    SmallString<256> Resolved;
    std::error_code EC = sys::fs::real_path(Config_.SandboxDir, Resolved);
    if (EC) {
      // SECURITY: If we can't resolve the sandbox to a real path, we MUST
      // reject it. An unresolved path could allow sandbox escapes via symlinks
      // or path traversal. Clear it to deny all file operations.
      reportViolation(ViolationType::SandboxEscape, Config_.SandboxDir);
      Config_.SandboxDir.clear();
      return;
    }
    Config_.SandboxDir = std::string(Resolved);

    // Ensure it ends with a separator for prefix matching
    if (!Config_.SandboxDir.empty() &&
        !sys::path::is_separator(Config_.SandboxDir.back())) {
      Config_.SandboxDir += sys::path::get_separator();
    }
  }
}

Expected<std::string> PathValidator::validate(StringRef Path,
                                              bool ForWrite) const {
  // Check for null bytes
  if (Path.contains('\0')) {
    reportViolation(ViolationType::NullByte, Path);
    return createStringError(inconvertibleErrorCode(),
                             "Path contains null byte");
  }

  // Check read-only mode
  if (ForWrite && Config_.ReadOnly) {
    reportViolation(ViolationType::WriteProtected, Path);
    return createStringError(inconvertibleErrorCode(),
                             "Write access denied (read-only mode)");
  }

  return resolveAndCheck(Path, ForWrite);
}

std::string PathValidator::normalizePath(StringRef Path) const {
  SmallString<256> Result;

  // If relative, prepend sandbox dir
  if (!sys::path::is_absolute(Path) && !Config_.SandboxDir.empty()) {
    Result = Config_.SandboxDir;
    sys::path::append(Result, Path);
  } else {
    Result = Path;
  }

  // Normalize (remove ".", "..", etc.)
  SmallString<256> Normalized;
  for (auto It = sys::path::begin(Result), End = sys::path::end(Result);
       It != End; ++It) {
    StringRef Component = *It;

    if (Component == ".")
      continue;

    if (Component == "..") {
      // Go up one level if possible
      if (!Normalized.empty()) {
        sys::path::remove_filename(Normalized);
      }
      continue;
    }

    sys::path::append(Normalized, Component);
  }

  return std::string(Normalized);
}

Expected<std::string> PathValidator::resolveAndCheck(StringRef Path,
                                                     bool ForWrite) const {
  // Normalize the path
  std::string NormalizedPath = normalizePath(Path);

  // Resolve symlinks to get the real path
  SmallString<256> RealPath;
  std::error_code EC = sys::fs::real_path(NormalizedPath, RealPath);

  // If the file doesn't exist yet (for create operations), check the parent
  if (EC == std::errc::no_such_file_or_directory && ForWrite) {
    SmallString<256> ParentPath(NormalizedPath);
    sys::path::remove_filename(ParentPath);
    if (!ParentPath.empty()) {
      SmallString<256> RealParent;
      EC = sys::fs::real_path(ParentPath, RealParent);
      if (!EC) {
        sys::path::append(RealParent, sys::path::filename(NormalizedPath));
        RealPath = RealParent;
      }
    }
  }

  // SECURITY: If we can't resolve the path (or parent for new files), deny.
  // Using an unverified normalized path could allow sandbox escapes via
  // symlink races or path traversal.
  if (EC) {
    reportViolation(ViolationType::SandboxEscape, Path);
    return createStringError(inconvertibleErrorCode(),
                             "Cannot verify path is within sandbox");
  }

  // Check if allowed
  if (!isAllowed(RealPath, ForWrite)) {
    reportViolation(ForWrite ? ViolationType::WriteProtected
                             : ViolationType::NotAllowed,
                    Path);
    return createStringError(inconvertibleErrorCode(), "Access denied");
  }

  return std::string(RealPath);
}

bool PathValidator::isAllowed(StringRef ResolvedPath, bool ForWrite) const {
  // Check sandbox directory (stored with trailing separator, e.g., "/sandbox/")
  if (!Config_.SandboxDir.empty()) {
    StringRef SandboxNoSep = StringRef(Config_.SandboxDir).drop_back();
    // Allow the sandbox dir itself or anything under it
    if (ResolvedPath == SandboxNoSep ||
        ResolvedPath.starts_with(Config_.SandboxDir)) {
      return true;
    }
  }

  // Check additional allowed paths
  for (const auto &[Prefix, AllowWrite] : Config_.AllowedPaths) {
    if (ResolvedPath.starts_with(Prefix)) {
      if (!ForWrite || AllowWrite)
        return true;
    }
  }

  return false;
}

void PathValidator::reportViolation(ViolationType Type, StringRef Path) const {
  if (Config_.OnViolation) {
    Config_.OnViolation(Type, Path);
  }
}

void PathValidator::addAllowedPath(StringRef Prefix, bool AllowWrite) {
  // Resolve and normalize the prefix
  SmallString<256> Resolved;
  if (sys::fs::real_path(Prefix, Resolved)) {
    Resolved = Prefix;
  }

  Config_.AllowedPaths.emplace_back(std::string(Resolved), AllowWrite);
}
