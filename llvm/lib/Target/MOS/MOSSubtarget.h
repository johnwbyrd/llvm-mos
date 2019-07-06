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
  bool hasSRAM() const { return m_hasSRAM; }
  bool hasJMPCALL() const { return m_hasJMPCALL; }
  bool hasIJMPCALL() const { return m_hasIJMPCALL; }
  bool hasEIJMPCALL() const { return m_hasEIJMPCALL; }
  bool hasADDSUBIW() const { return m_hasADDSUBIW; }
  bool hasSmallStack() const { return m_hasSmallStack; }
  bool hasMOVW() const { return m_hasMOVW; }
  bool hasLPM() const { return m_hasLPM; }
  bool hasLPMX() const { return m_hasLPMX; }
  bool hasELPM() const { return m_hasELPM; }
  bool hasELPMX() const { return m_hasELPMX; }
  bool hasSPM() const { return m_hasSPM; }
  bool hasSPMX() const { return m_hasSPMX; }
  bool hasDES() const { return m_hasDES; }
  bool supportsRMW() const { return m_supportsRMW; }
  bool supportsMultiplication() const { return m_supportsMultiplication; }
  bool hasBREAK() const { return m_hasBREAK; }
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
  bool m_hasSRAM;
  bool m_hasJMPCALL;
  bool m_hasIJMPCALL;
  bool m_hasEIJMPCALL;
  bool m_hasADDSUBIW;
  bool m_hasSmallStack;
  bool m_hasMOVW;
  bool m_hasLPM;
  bool m_hasLPMX;
  bool m_hasELPM;
  bool m_hasELPMX;
  bool m_hasSPM;
  bool m_hasSPMX;
  bool m_hasDES;
  bool m_supportsRMW;
  bool m_supportsMultiplication;
  bool m_hasBREAK;
  bool m_hasTinyEncoding;

  /// The ELF e_flags architecture.
  unsigned ELFArch;

  // Dummy member, used by FeatureSet's. We cannot have a SubtargetFeature with
  // no variable, so we instead bind pseudo features to this variable.
  bool m_FeatureSetDummy;
};

} // end namespace llvm

#endif // LLVM_MOS_SUBTARGET_H
