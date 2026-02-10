//===-- FileBackend.cpp - File Backend Implementation ----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/FileBackend.h"
#include <cerrno>
#include <cstdio>

using namespace llvm;
using namespace llvm::emu::semihost;

OpResult FileBackend::open(StringRef Path, OpenMode Mode) {
  const char *ModeStr = openModeToString(Mode);
  if (!ModeStr) {
    LastErrno = EINVAL;
    return OpResult::error(EINVAL);
  }

  // Path is already resolved by Policy layer
  std::string PathStr = resolvePath(Path);
  FILE *FP = std::fopen(PathStr.c_str(), ModeStr);
  if (!FP) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  int FD = FDTable_.allocate(FP);
  if (FD < 0) {
    std::fclose(FP);
    LastErrno = EMFILE;
    return OpResult::error(EMFILE);
  }

  return OpResult::success(FD);
}

OpResult FileBackend::close(int FD) {
  // Don't close stdin/stdout/stderr
  if (FDTable_.isStdio(FD)) {
    return OpResult::success();
  }

  if (!FDTable_.isValid(FD)) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }

  FDTable_.release(FD);
  return OpResult::success();
}

OpResult FileBackend::read(int FD, size_t Count) {
  // Handle stdin specially through ConsoleBackend
  if (FD == 0)
    return ConsoleBackend::read(FD, Count);

  FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }

  OpResult Result;
  Result.Data.resize(Count);

  size_t BytesRead = std::fread(Result.Data.data(), 1, Count, FP);
  Result.Data.resize(BytesRead);

  if (BytesRead < Count && std::ferror(FP)) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  // Return bytes NOT read (ARM semihosting convention)
  Result.Value = static_cast<intmax_t>(Count - BytesRead);
  Result.Errno = 0;
  return Result;
}

OpResult FileBackend::write(int FD, ArrayRef<uint8_t> Data) {
  // Handle stdout/stderr specially through ConsoleBackend
  if (FD == 1 || FD == 2)
    return ConsoleBackend::write(FD, Data);

  FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }

  size_t BytesWritten = std::fwrite(Data.data(), 1, Data.size(), FP);

  if (BytesWritten < Data.size()) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  // Return bytes NOT written (ARM semihosting convention)
  return OpResult::success(static_cast<intmax_t>(Data.size() - BytesWritten));
}

OpResult FileBackend::seek(int FD, int64_t Pos) {
  FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }

  if (std::fseek(FP, static_cast<long>(Pos), SEEK_SET) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  return OpResult::success();
}

OpResult FileBackend::fileLength(int FD) {
  FILE *FP = FDTable_.get(FD);
  if (!FP) {
    LastErrno = EBADF;
    return OpResult::error(EBADF);
  }

  // Save current position
  long CurPos = std::ftell(FP);
  if (CurPos < 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  // Seek to end
  if (std::fseek(FP, 0, SEEK_END) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  // Get position (= file length)
  long Length = std::ftell(FP);
  if (Length < 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  // Restore position
  std::fseek(FP, CurPos, SEEK_SET);

  return OpResult::success(Length);
}

OpResult FileBackend::remove(StringRef Path) {
  // Path is already resolved by Policy layer
  std::string PathStr = resolvePath(Path);
  if (std::remove(PathStr.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  return OpResult::success();
}

OpResult FileBackend::rename(StringRef OldPath, StringRef NewPath) {
  // Paths are already resolved by Policy layer
  std::string OldPathStr = resolvePath(OldPath);
  std::string NewPathStr = resolvePath(NewPath);

  if (std::rename(OldPathStr.c_str(), NewPathStr.c_str()) != 0) {
    LastErrno = errno;
    return OpResult::error(errno);
  }

  return OpResult::success();
}

OpResult FileBackend::tmpnam(int Id) {
  // Generate a unique temporary filename
  char Buf[64];
  std::snprintf(Buf, sizeof(Buf), "tmp%05d_%03d", TmpNameCounter_++, Id);

  OpResult Result;
  Result.Value = 0;
  Result.Errno = 0;
  Result.Data.assign(reinterpret_cast<uint8_t *>(Buf),
                     reinterpret_cast<uint8_t *>(Buf) + std::strlen(Buf) + 1);
  return Result;
}

bool FileBackend::isTTY(int FD) {
  // stdin/stdout/stderr are TTYs
  if (FDTable_.isStdio(FD))
    return true;

  // Regular files are not TTYs
  return false;
}
