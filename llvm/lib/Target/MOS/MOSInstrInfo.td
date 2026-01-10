//===-- MOSInstrInfo.td - MOS Instruction defs -------------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the MOS instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "MOSInstrFormats.td"

//===----------------------------------------------------------------------===//
// Hardware Instructions (organized by CPU family)
//===----------------------------------------------------------------------===//

include "MOSInstr6502.td"
include "MOSInstr6502X.td"
include "MOSInstr65C02.td"
include "MOSInstr65CE02.td"
include "MOSInstrHuC6280.td"
include "MOSInstrW65816.td"

// Operand type for basic block and function labels.
def label : Operand<OtherVT>;

include "MOSInstrInfoTables.td"

include "MOSInstrInfoSPC700.td"

include "MOSInstrPseudos.td"
include "MOSInstrLogical.td"
include "MOSInstrGISel.td"
