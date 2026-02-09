//===-- Memory.cpp - RAM/ROM Device Implementation --------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Memory.h"
#include "llvm/Object/ObjectFile.h"

using namespace llvm;
using namespace llvm::emu;

Error Memory::loadObject(const object::ObjectFile &Obj, Memory &Mem) {
  for (const object::SectionRef &Section : Obj.sections()) {
    // Skip non-loadable sections
    if (!Section.isText() && !Section.isData())
      continue;

    // BSS sections are zero-initialized (Memory starts zeroed)
    if (Section.isBSS())
      continue;

    Expected<StringRef> Contents = Section.getContents();
    if (!Contents)
      return Contents.takeError();

    if (!Contents->empty()) {
      Mem.writeBlock(Section.getAddress(),
                     reinterpret_cast<const uint8_t *>(Contents->data()),
                     Contents->size());
    }
  }
  return Error::success();
}
