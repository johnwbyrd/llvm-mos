//===- MCEmulator.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCEmulator/MCEmulator.h"

namespace llvm {

int it = 0;

void MCEmulator::test()
{
    int i = 1;
    assert(i == 1);
}

} // namespace llvm
