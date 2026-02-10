//===-- Policy.cpp - Security Policy Implementation -----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Semihost/Policy.h"

using namespace llvm;
using namespace llvm::emu::semihost;

//===----------------------------------------------------------------------===//
// SandboxedPolicy
//===----------------------------------------------------------------------===//

SandboxedPolicy::SandboxedPolicy(StringRef SandboxDir) {
  PathValidatorConfig Config;
  Config.SandboxDir = SandboxDir.str();
  Validator = std::make_unique<PathValidator>(std::move(Config));
}

Expected<std::string> SandboxedPolicy::resolvePath(StringRef Path,
                                                    bool ForWrite) {
  return Validator->validate(Path, ForWrite);
}

void SandboxedPolicy::addAllowedPath(StringRef Prefix, bool AllowWrite) {
  Validator->addAllowedPath(Prefix, AllowWrite);
}
