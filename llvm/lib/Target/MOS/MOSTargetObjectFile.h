//===-- MOSTargetObjectFile.h - MOS Object Info -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MOS_TARGET_OBJECT_FILE_H
#define LLVM_MOS_TARGET_OBJECT_FILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

/// Lowering for an MOS ELF32 object file.
class MOSTargetObjectFile : public TargetLoweringObjectFileELF {
  typedef TargetLoweringObjectFileELF Base;

public:
  void Initialize(MCContext &ctx, const TargetMachine &TM) override;

  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override;

private:
  MCSection *ProgmemDataSection;
};

} // end namespace llvm

#endif // LLVM_MOS_TARGET_OBJECT_FILE_H
