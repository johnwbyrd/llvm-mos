//===-- MOSSubtarget.h - Define Subtarget for the MOS -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the MOS specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MOS_SUBTARGET_H
#define LLVM_MOS_SUBTARGET_H

#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#include "MOSFrameLowering.h"
#include "MOSISelLowering.h"
#include "MOSInstrInfo.h"
#include "MOSSelectionDAGInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "MOSGenSubtargetInfo.inc"

namespace llvm {

/// A specific MOS target MCU.
class MOSSubtarget : public MOSGenSubtargetInfo {
public:
  //! Creates an MOS subtarget.
  //! \param TT  The target triple.
  //! \param CPU The CPU to target.
  //! \param FS  The feature string.
  //! \param TM  The target machine.
  MOSSubtarget(const Triple &TT, const std::string &CPU, const std::string &FS,
               const MOSTargetMachine &TM);

  const MOSInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const TargetFrameLowering *getFrameLowering() const override { return &FrameLowering; }
  const MOSTargetLowering *getTargetLowering() const override { return &TLInfo; }
  const MOSSelectionDAGInfo *getSelectionDAGInfo() const override { return &TSInfo; }
  const MOSRegisterInfo *getRegisterInfo() const override { return &InstrInfo.getRegisterInfo(); }

  /// Parses a subtarget feature string, setting appropriate options.
  /// \note Definition of function is auto generated by `tblgen`.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);

  MOSSubtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                                const TargetMachine &TM);

  // Subtarget feature getters.
  // See MOS.td for details.
  bool hasGreenInsns() const { return m_hasGreenInsns; }
  bool hasYellowInsns() const { return m_hasYellowInsns; }
  bool hasRedInsns() const { return m_hasRedInsns; }
  bool hasSweet16Insn() const {return m_hasSweet16Insns; }

  bool hasTinyEncoding() const { return m_hasTinyEncoding; }

  /// Gets the ELF architecture for the e_flags field
  /// of an ELF object file.
  unsigned getELFArch() const {
    assert(ELFArch != 0 &&
           "every device must have an associate ELF architecture");
    return ELFArch;
  }

private:
  MOSInstrInfo InstrInfo;
  MOSFrameLowering FrameLowering;
  MOSTargetLowering TLInfo;
  MOSSelectionDAGInfo TSInfo;

  // Subtarget feature settings
  // See MOS.td for details.
  bool m_hasTinyEncoding;

  bool m_hasGreenInsns;
  bool m_hasYellowInsns;
  bool m_hasRedInsns;

  bool m_hasSweet16Insns;

  /// The ELF e_flags architecture.
  unsigned ELFArch;

  // Dummy member, used by FeatureSet's. We cannot have a SubtargetFeature with
  // no variable, so we instead bind pseudo features to this variable.
  bool m_FeatureSetDummy;
};

} // end namespace llvm

#endif // LLVM_MOS_SUBTARGET_H
