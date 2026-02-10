//===-- llvm/Emulator/Semihost/FileBackend.h - File Backend ----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// File backend that adds file operations to ConsoleBackend.
/// Subclasses implement path validation for secure/insecure access.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_FILEBACKEND_H
#define LLVM_EMULATOR_SEMIHOST_FILEBACKEND_H

#include "llvm/Emulator/Semihost/Backend.h"
#include "llvm/Emulator/Semihost/FileDescTable.h"
#include "llvm/Support/Error.h"
#include <string>

namespace llvm {
namespace emu {
namespace semihost {

/// Backend that supports file operations via FileDescTable.
///
/// This class implements all file operations but delegates path
/// resolution to the virtual resolvePath() method. Subclasses
/// implement security policy by overriding resolvePath().
class FileBackend : public ConsoleBackend {
public:
  FileBackend(ExitCallback OnExit, TimerCallback OnTimer = nullptr)
      : ConsoleBackend(std::move(OnExit), std::move(OnTimer)) {}

  // File operations
  OpResult open(StringRef Path, OpenMode Mode) override;
  OpResult close(int FD) override;
  OpResult read(int FD, size_t Count) override;
  OpResult write(int FD, ArrayRef<uint8_t> Data) override;
  OpResult seek(int FD, int64_t Pos) override;
  OpResult fileLength(int FD) override;
  OpResult remove(StringRef Path) override;
  OpResult rename(StringRef OldPath, StringRef NewPath) override;
  OpResult tmpnam(int Id) override;

  // Query operations
  bool isTTY(int FD) override;

protected:
  /// Resolve and validate a path.
  /// Subclasses override this to implement security policy.
  /// @param Path The requested path.
  /// @param ForWrite True if the operation will write to the path.
  /// @return Resolved path on success, Error if access denied.
  virtual Expected<std::string> resolvePath(StringRef Path, bool ForWrite) = 0;

  FileDescTable FDTable_;
  int TmpNameCounter_ = 0;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_FILEBACKEND_H
