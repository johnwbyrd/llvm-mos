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
  MOSSubtarget(const Triple &TT, const std::string &CPU, const std::string &FS,
               const MOSTargetMachine &TM);

  /// Gets the ELF architecture for the e_flags field
  /// of an ELF object file.
  unsigned getELFArch() const {
    assert(ELFArch != 0 &&
           "every device must have an associate ELF architecture");
    return ELFArch;
  }

  const TargetFrameLowering *getFrameLowering() const override;
  const MOSInstrInfo *getInstrInfo() const override;
  const MOSRegisterInfo *getRegisterInfo() const override;

  const MOSSelectionDAGInfo *getSelectionDAGInfo() const override;
  const MOSTargetLowering *getTargetLowering() const override;
  // Subtarget feature getters.
  // See MOS.td for details.
  bool hasTinyEncoding() const { return m_hasTinyEncoding; }

  MOSSubtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                                const TargetMachine &TM);

  /// Parses a subtarget feature string, setting appropriate options.
  /// \note Definition of function is auto generated by `tblgen`.
  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

private:
  MOSInstrInfo InstrInfo;
  MOSFrameLowering FrameLowering;
  MOSTargetLowering TLInfo;
  MOSSelectionDAGInfo TSInfo;

  // Subtarget feature settings
  // See MOS.td for details.
  bool m_hasTinyEncoding;

  bool m_Has6502Insns;
  bool m_Has6502BCDInsns;
  bool m_Has6502XInsns;
  bool m_Has65C02Insns;
  bool m_HasR65C02Insns;
  bool m_HasW65C02Insns;
  bool m_HasW65816Insns;
  bool m_Has65EL02Insns;
  bool m_Has65CE02Insns;
  bool m_HasSWEET16Insns;

  bool m_LongRegisterNames;

  /// The ELF e_flags architecture.
  unsigned ELFArch;

  // Dummy member, used by FeatureSet's. We cannot have a SubtargetFeature with
  // no variable, so we instead bind pseudo features to this variable.
  bool m_FeatureSetDummy;
};

} // end namespace llvm

#endif // LLVM_MOS_SUBTARGET_H
