//===-- Backend.cpp - Backend Implementation --------------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/Backend.h"
#include <chrono>
#include <cstdio>
#include <ctime>

using namespace llvm;
using namespace llvm::emu::semihost;

//===----------------------------------------------------------------------===//
// ConsoleBackend Implementation
//===----------------------------------------------------------------------===//

void ConsoleBackend::writeChar(char C) { std::putchar(C); }

void ConsoleBackend::writeString(StringRef Str) {
  std::fwrite(Str.data(), 1, Str.size(), stdout);
}

int ConsoleBackend::readChar() { return std::getchar(); }

OpResult ConsoleBackend::read(int FD, size_t Count) {
  if (FD != 0) // Only stdin supported
    return OpResult::error(EBADF);

  OpResult Result;
  Result.Data.resize(Count);

  size_t BytesRead = std::fread(Result.Data.data(), 1, Count, stdin);
  Result.Data.resize(BytesRead);

  // Return bytes NOT read (ARM semihosting convention)
  Result.Value = static_cast<intmax_t>(Count - BytesRead);
  Result.Errno = 0;
  return Result;
}

OpResult ConsoleBackend::write(int FD, ArrayRef<uint8_t> Data) {
  FILE *Stream = nullptr;
  switch (FD) {
  case 1:
    Stream = stdout;
    break;
  case 2:
    Stream = stderr;
    break;
  default:
    return OpResult::error(EBADF);
  }

  size_t BytesWritten = std::fwrite(Data.data(), 1, Data.size(), Stream);
  std::fflush(Stream);

  // Return bytes NOT written (ARM semihosting convention)
  return OpResult::success(static_cast<intmax_t>(Data.size() - BytesWritten));
}

OpResult ConsoleBackend::clock() {
  auto Now = std::chrono::steady_clock::now();
  auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      Now - StartTime_);
  // Return centiseconds (100ths of a second)
  return OpResult::success(static_cast<intmax_t>(Elapsed.count() / 10));
}

OpResult ConsoleBackend::time() {
  return OpResult::success(static_cast<intmax_t>(std::time(nullptr)));
}

void ConsoleBackend::exit(unsigned Reason, unsigned Subcode) {
  if (OnExit_)
    OnExit_(Reason, Subcode);
}

OpResult ConsoleBackend::timerConfig(unsigned RateHz) {
  if (OnTimer_) {
    OnTimer_(RateHz);
    return OpResult::success();
  }
  return OpResult::error(ENOSYS);
}
