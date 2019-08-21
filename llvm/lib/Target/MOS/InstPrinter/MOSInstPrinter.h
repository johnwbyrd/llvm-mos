//===- MOSInstPrinter.h - Convert MOS MCInst to assembly syntax -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class prints an MOS MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MOS_INST_PRINTER_H
#define LLVM_MOS_INST_PRINTER_H

#include "llvm/MC/MCInstPrinter.h"

#include "MCTargetDesc/MOSMCTargetDesc.h"

namespace llvm {

/// Prints MOS instructions to a textual stream.
class MOSInstPrinter : public MCInstPrinter {
public:
  MOSInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                 const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}
};

} // end namespace llvm

#endif // LLVM_MOS_INST_PRINTER_H

