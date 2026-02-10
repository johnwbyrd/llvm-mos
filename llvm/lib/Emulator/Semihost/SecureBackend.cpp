//===-- SecureBackend.cpp - Secure Backend Implementation -----*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/SecureBackend.h"
#include <cerrno>
#include <cstdlib>

using namespace llvm;
using namespace llvm::emu::semihost;

//===----------------------------------------------------------------------===//
// SecureBackend
//===----------------------------------------------------------------------===//

SecureBackend::SecureBackend(PathValidatorConfig ValidatorConfig,
                             ExitCallback OnExit, TimerCallback OnTimer)
    : FileBackend(std::move(OnExit), std::move(OnTimer)),
      Validator_(std::move(ValidatorConfig)) {}

Expected<std::string> SecureBackend::resolvePath(StringRef Path,
                                                 bool ForWrite) {
  return Validator_.validate(Path, ForWrite);
}

void SecureBackend::addAllowedPath(StringRef Prefix, bool AllowWrite) {
  Validator_.addAllowedPath(Prefix, AllowWrite);
}

OpResult SecureBackend::system(StringRef Command) {
  if (!Validator_.allowSystem()) {
    LastErrno = ENOSYS;
    return OpResult::error(ENOSYS);
  }

  // Execute the command
  std::string CmdStr(Command);
  int Result = std::system(CmdStr.c_str());

  return OpResult::success(Result);
}

//===----------------------------------------------------------------------===//
// InsecureBackend
//===----------------------------------------------------------------------===//

Expected<std::string> InsecureBackend::resolvePath(StringRef Path,
                                                   bool ForWrite) {
  // No validation - return path as-is
  return std::string(Path);
}

OpResult InsecureBackend::system(StringRef Command) {
  // No restrictions - execute any command
  std::string CmdStr(Command);
  int Result = std::system(CmdStr.c_str());

  return OpResult::success(Result);
}
