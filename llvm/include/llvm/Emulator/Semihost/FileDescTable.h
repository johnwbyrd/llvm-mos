//===-- llvm/Emulator/Semihost/FileDescTable.h - FD Pool -------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// RAII file descriptor table for semihosting.
/// Manages open FILE* handles and maps them to integer descriptors.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_SEMIHOST_FILEDESCTABLE_H
#define LLVM_EMULATOR_SEMIHOST_FILEDESCTABLE_H

#include <array>
#include <cstdio>

namespace llvm {
namespace emu {
namespace semihost {

/// RAII file descriptor table.
///
/// Maps integer file descriptors to FILE* handles.
/// FDs 0, 1, 2 are reserved for stdin, stdout, stderr.
/// User files get FDs starting from 3.
class FileDescTable {
public:
  static constexpr int MaxFiles = 64;
  static constexpr int FirstUserFD = 3;

  FileDescTable();
  ~FileDescTable();

  // Non-copyable, movable
  FileDescTable(const FileDescTable &) = delete;
  FileDescTable &operator=(const FileDescTable &) = delete;
  FileDescTable(FileDescTable &&Other) noexcept;
  FileDescTable &operator=(FileDescTable &&Other) noexcept;

  /// Allocate a new file descriptor for the given FILE*.
  /// @return FD on success, -1 if table is full.
  int allocate(FILE *FP);

  /// Release a file descriptor and close the associated file.
  /// Does nothing for stdin/stdout/stderr (FDs 0-2).
  /// @return true if FD was valid and released.
  bool release(int FD);

  /// Get the FILE* for a file descriptor.
  /// @return FILE* or nullptr if FD is invalid.
  FILE *get(int FD) const;

  /// Check if a file descriptor is valid.
  bool isValid(int FD) const;

  /// Check if a file descriptor refers to stdin/stdout/stderr.
  bool isStdio(int FD) const { return FD >= 0 && FD < FirstUserFD; }

  /// Close all open files (except stdin/stdout/stderr).
  void closeAll();

private:
  std::array<FILE *, MaxFiles> Files_;
};

} // namespace semihost
} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_SEMIHOST_FILEDESCTABLE_H
