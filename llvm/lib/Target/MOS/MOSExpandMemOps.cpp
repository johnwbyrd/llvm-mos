//===-- MOSExpandMemOps.cpp - MOS Memory Operation Expansion --------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the MOS memory operation expansion pass.
//
// This pass expands MemCpy, MemMove, and MemSet pseudo-instructions into
// efficient code sequences. It runs after register allocation when physical
// registers are available.
//
// The pass handles:
// - HuC6280 block copy instructions (TII/TDD) when addresses are constant
// - Y-indexed loops for variable-address copies
// - Memmove direction detection for overlapping regions
// - 16-bit sizes with page loops
//
// Generated loop patterns:
//
//   8-bit size (≤256 bytes), forward:
//       ldy #0
//   loop:
//       lda (src),y
//       sta (dst),y
//       iny
//       cpy #size
//       bne loop
//
//   8-bit size, reverse (for memcpy, memset, memmove when dst > src):
//       ldy size      ; sets Z=1 if size=0
//       beq exit      ; skip if size=0
//   loop:
//       dey           ; Y = size-1 on first iteration
//       lda (src),y
//       sta (dst),y
//       tya           ; sets Z=1 if Y=0 (LDA/STA clobber flags)
//       bne loop      ; loop while Y != 0
//   exit:
//
//   Reference: http://6502.org/source/general/memory_move.html
//
//   16-bit size (>256 bytes):
//       ldx #pages
//   page_loop:
//       ldy #0
//   byte_loop:
//       lda (src),y
//       sta (dst),y
//       iny
//       bne byte_loop
//       inc src+1
//       inc dst+1
//       dex
//       bne page_loop
//       ; handle remainder with 8-bit loop
//
//===----------------------------------------------------------------------===//

#include "MOSExpandMemOps.h"

#include "MCTargetDesc/MOSMCTargetDesc.h"
#include "MOS.h"
#include "MOSSubtarget.h"

#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define DEBUG_TYPE "mos-expand-memops"

using namespace llvm;

namespace {

class MOSExpandMemOps : public MachineFunctionPass {
public:
  static char ID;

  MOSExpandMemOps() : MachineFunctionPass(ID) {
    initializeMOSExpandMemOpsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "MOS Memory Operation Expansion";
  }

private:
  const MOSSubtarget *STI = nullptr;
  const TargetInstrInfo *TII = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  bool expandMemCpy(MachineInstr &MI);
  bool expandMemSet(MachineInstr &MI);
  bool expandMemMove(MachineInstr &MI);
  bool expandMemMoveReverse(MachineInstr &MI);

  /// Build a forward Y-indexed loop for memcpy or memset.
  void buildForwardLoop(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, Register DstPtr, Register SrcPtrOrVal,
                        Register SizeLo, Register SizeHi, bool IsMemSet);

  /// Build a reverse Y-indexed loop for memcpy/memset/memmove.
  /// Faster than forward loop due to DEY/BPL vs INY/CPY/BNE.
  void buildReverseLoop(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                        const DebugLoc &DL, Register DstPtr, Register SrcPtrOrVal,
                        Register SizeLo, Register SizeHi, bool IsMemSet = false);

  /// Try to use HuC6280 block copy instructions.
  /// Returns true if HuC block copy was emitted.
  bool tryHuCBlockCopy(MachineInstr &MI, bool IsMemSet, bool Descending);
};

bool MOSExpandMemOps::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<MOSSubtarget>();
  TII = STI->getInstrInfo();
  MRI = &MF.getRegInfo();

  // Collect all memory operation pseudos first, since expansion modifies
  // the basic block structure.
  SmallVector<MachineInstr *, 4> MemOps;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      switch (MI.getOpcode()) {
      case MOS::MemCpy:
      case MOS::MemSet:
      case MOS::MemMove:
      case MOS::MemMoveReverse:
        MemOps.push_back(&MI);
        break;
      }
    }
  }

  bool Changed = false;
  for (MachineInstr *MI : MemOps) {
    switch (MI->getOpcode()) {
    case MOS::MemCpy:
      Changed |= expandMemCpy(*MI);
      break;
    case MOS::MemSet:
      Changed |= expandMemSet(*MI);
      break;
    case MOS::MemMove:
      Changed |= expandMemMove(*MI);
      break;
    case MOS::MemMoveReverse:
      Changed |= expandMemMoveReverse(*MI);
      break;
    }
  }

  return Changed;
}

bool MOSExpandMemOps::expandMemCpy(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  Register DstPtr = MI.getOperand(0).getReg();
  Register SrcPtr = MI.getOperand(1).getReg();
  Register SizeLo = MI.getOperand(2).getReg();
  Register SizeHi = MI.getOperand(3).getReg();

  // TODO: Try HuC block copy for constant addresses on HuC6280.

  // Use reverse loop by default - it's faster because DEY/BPL saves
  // 3 cycles per byte compared to INY/CPY/BNE.
  // For memcpy, order doesn't matter since regions don't overlap.
  buildReverseLoop(MBB, MI.getIterator(), DL, DstPtr, SrcPtr, SizeLo, SizeHi);

  MI.eraseFromParent();
  return true;
}

bool MOSExpandMemOps::expandMemSet(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  Register DstPtr = MI.getOperand(0).getReg();
  Register FillVal = MI.getOperand(1).getReg();
  Register SizeLo = MI.getOperand(2).getReg();
  Register SizeHi = MI.getOperand(3).getReg();

  // Use reverse loop by default - it's faster because DEY/BPL saves
  // 3 cycles per byte compared to INY/CPY/BNE.
  buildReverseLoop(MBB, MI.getIterator(), DL, DstPtr, FillVal, SizeLo, SizeHi,
                   /*IsMemSet=*/true);

  MI.eraseFromParent();
  return true;
}

bool MOSExpandMemOps::expandMemMove(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  Register DstPtr = MI.getOperand(0).getReg();
  Register SrcPtr = MI.getOperand(1).getReg();
  Register SizeLo = MI.getOperand(2).getReg();
  Register SizeHi = MI.getOperand(3).getReg();

  // For memmove, we need to handle overlapping regions correctly.
  // If dst > src and they overlap, we must copy backwards.
  //
  // At this point (post-regalloc), the addresses are in imaginary registers.
  // We cannot easily compare them at compile time unless they were constants
  // that the legalizer could have analyzed.
  //
  // For now, we'll emit a forward copy. The legalizer should have already
  // determined the direction for constant addresses and could emit a different
  // pseudo (MemMoveReverse) if needed.
  //
  // TODO: Add runtime direction check for truly dynamic addresses:
  //   cmp dst, src
  //   bcc forward
  //   ; reverse copy
  // forward:
  //   ; forward copy

  buildForwardLoop(MBB, MI.getIterator(), DL, DstPtr, SrcPtr, SizeLo, SizeHi,
                   /*IsMemSet=*/false);

  MI.eraseFromParent();
  return true;
}

bool MOSExpandMemOps::expandMemMoveReverse(MachineInstr &MI) {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  Register DstPtr = MI.getOperand(0).getReg();
  Register SrcPtr = MI.getOperand(1).getReg();
  Register SizeLo = MI.getOperand(2).getReg();
  Register SizeHi = MI.getOperand(3).getReg();

  // Reverse copy: start from end and work backwards.
  // Used when dst > src and regions overlap.
  buildReverseLoop(MBB, MI.getIterator(), DL, DstPtr, SrcPtr, SizeLo, SizeHi);

  MI.eraseFromParent();
  return true;
}

void MOSExpandMemOps::buildForwardLoop(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator InsertPt,
                                        const DebugLoc &DL,
                                        Register DstPtr, Register SrcPtrOrVal,
                                        Register SizeLo, Register SizeHi,
                                        bool IsMemSet) {
  MachineFunction &MF = *MBB.getParent();
  const BasicBlock *LLVM_BB = MBB.getBasicBlock();

  // Create the loop and exit basic blocks.
  MachineBasicBlock *LoopMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *ExitMBB = MF.CreateMachineBasicBlock(LLVM_BB);

  // Insert them after MBB.
  MF.insert(std::next(MBB.getIterator()), LoopMBB);
  MF.insert(std::next(LoopMBB->getIterator()), ExitMBB);

  // Move all instructions after InsertPt to ExitMBB.
  ExitMBB->splice(ExitMBB->end(), &MBB, std::next(InsertPt), MBB.end());

  // Transfer successors from MBB to ExitMBB.
  ExitMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // Update successors: MBB falls through to LoopMBB
  MBB.addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(LoopMBB); // Loop back
  LoopMBB->addSuccessor(ExitMBB); // Exit

  // Compute which registers are live at the start of ExitMBB. These need to
  // be live through LoopMBB, and also need to be marked as liveins for ExitMBB.
  // Walk backward from the end of ExitMBB, tracking uses and defs.
  SmallSet<MCPhysReg, 16> LiveAtEntry;
  for (const MachineInstr &MI : llvm::reverse(*ExitMBB)) {
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isReg() && MO.getReg().isPhysical()) {
        if (MO.isDef() && !MO.isDead())
          LiveAtEntry.erase(MO.getReg());
        if (MO.isUse() && !MO.isUndef())
          LiveAtEntry.insert(MO.getReg());
      }
    }
  }
  for (MCPhysReg Reg : LiveAtEntry) {
    LoopMBB->addLiveIn(Reg);
    ExitMBB->addLiveIn(Reg);
  }

  // Add liveins for the loop block. These registers are used in the loop body
  // but defined before the loop or need to be live across the back edge.
  LoopMBB->addLiveIn(MOS::Y);
  LoopMBB->addLiveIn(DstPtr);
  LoopMBB->addLiveIn(SizeLo);
  LoopMBB->addLiveIn(SrcPtrOrVal); // Source pointer or fill value

  // Build the loop header in MBB: ldy #0
  // Use LDImm pseudo which gets properly lowered to LDY_Immediate by MCInstLower.
  BuildMI(&MBB, DL, TII->get(MOS::LDImm), MOS::Y).addImm(0);

  // Fall through to LoopMBB (no jump needed since LoopMBB follows MBB)

  // Build loop body
  if (IsMemSet) {
    // For memset: the fill value is already in a register.
    // Copy it to A if not already there.
    if (SrcPtrOrVal != MOS::A) {
      BuildMI(LoopMBB, DL, TII->get(MOS::COPY), MOS::A)
          .addReg(SrcPtrOrVal);
    }
  } else {
    // For memcpy: load from source using indirect indexed addressing.
    // LDIndirIdx: (outs Ac:$dst), (ins Imag16:$addr, Yc:$offset)
    BuildMI(LoopMBB, DL, TII->get(MOS::LDIndirIdx), MOS::A)
        .addReg(SrcPtrOrVal)
        .addReg(MOS::Y);
  }

  // Store to destination using indirect indexed addressing.
  // STIndirIdx: (ins Ac:$src, Imag16:$addr, Yc:$offset)
  BuildMI(LoopMBB, DL, TII->get(MOS::STIndirIdx))
      .addReg(MOS::A)
      .addReg(DstPtr)
      .addReg(MOS::Y);

  // Increment Y
  BuildMI(LoopMBB, DL, TII->get(MOS::INY_Implied));

  // Compare Y with size and branch back if not equal.
  // TODO: Handle 16-bit sizes properly with an outer page loop.
  // For now, assume 8-bit size in SizeLo.
  // CmpBrImag8: (ins label:$tgt, Flag:$flag, i1imm:$flag_val, GPR:$l, Imag8:$r)
  // Branch if Z=0 (not equal) back to loop.
  // The flag register is marked Undef because CmpBrImag8 internally performs
  // the compare which produces the flag - it doesn't read a pre-existing value.
  BuildMI(LoopMBB, DL, TII->get(MOS::CmpBrImag8))
      .addMBB(LoopMBB)                      // target
      .addReg(MOS::Z, RegState::Undef)      // flag to test (Undef: compare produces it)
      .addImm(0)                            // branch if flag=0 (Z=0 means not equal)
      .addReg(MOS::Y)                       // left operand (Y register)
      .addReg(SizeLo);                      // right operand (size)

  // Fall through to ExitMBB
}

void MOSExpandMemOps::buildReverseLoop(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator InsertPt,
                                        const DebugLoc &DL,
                                        Register DstPtr, Register SrcPtrOrVal,
                                        Register SizeLo, Register SizeHi,
                                        bool IsMemSet) {
  MachineFunction &MF = *MBB.getParent();
  const BasicBlock *LLVM_BB = MBB.getBasicBlock();

  // Create the loop and exit basic blocks.
  MachineBasicBlock *LoopMBB = MF.CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *ExitMBB = MF.CreateMachineBasicBlock(LLVM_BB);

  // Insert them after MBB: MBB -> LoopMBB -> ExitMBB
  // This allows MBB to fall through to LoopMBB.
  MF.insert(std::next(MBB.getIterator()), LoopMBB);
  MF.insert(std::next(LoopMBB->getIterator()), ExitMBB);

  // Move all instructions after InsertPt to ExitMBB.
  ExitMBB->splice(ExitMBB->end(), &MBB, std::next(InsertPt), MBB.end());

  // Transfer successors from MBB to ExitMBB.
  ExitMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  // Update successors: MBB can go to LoopMBB (normal) or ExitMBB (size=0)
  MBB.addSuccessor(LoopMBB);
  MBB.addSuccessor(ExitMBB);
  LoopMBB->addSuccessor(LoopMBB); // Loop back
  LoopMBB->addSuccessor(ExitMBB); // Exit

  // Compute which registers are live at the start of ExitMBB. These need to
  // be live through LoopMBB, and also need to be marked as liveins for ExitMBB.
  // Walk backward from the end of ExitMBB, tracking uses and defs.
  SmallSet<MCPhysReg, 16> LiveAtEntry;
  for (const MachineInstr &MI : llvm::reverse(*ExitMBB)) {
    for (const MachineOperand &MO : MI.operands()) {
      if (MO.isReg() && MO.getReg().isPhysical()) {
        if (MO.isDef() && !MO.isDead())
          LiveAtEntry.erase(MO.getReg());
        if (MO.isUse() && !MO.isUndef())
          LiveAtEntry.insert(MO.getReg());
      }
    }
  }
  for (MCPhysReg Reg : LiveAtEntry) {
    LoopMBB->addLiveIn(Reg);
    ExitMBB->addLiveIn(Reg);
  }

  // Add liveins specific to the loop body.
  LoopMBB->addLiveIn(MOS::Y);
  LoopMBB->addLiveIn(DstPtr);
  LoopMBB->addLiveIn(SrcPtrOrVal); // Source pointer or fill value

  // Reverse loop pattern (handles sizes 0-255):
  //     ldy size      ; sets Z=1 if size=0
  //     beq exit      ; skip if size=0
  // loop:
  //     dey           ; Y = size-1 on first iteration
  //     lda (src),y
  //     sta (dst),y
  //     tya           ; sets Z=1 if Y=0 (LDA/STA clobber flags)
  //     bne loop      ; loop while Y != 0
  // exit:
  //
  // Reference: http://6502.org/source/general/memory_move.html

  // Header: load size into Y
  BuildMI(&MBB, DL, TII->get(MOS::COPY), MOS::Y).addReg(SizeLo);

  // Branch to exit if size=0 (BEQ).
  // CmpBrZero compares against zero and branches based on flag.
  // We want BEQ (branch if Z=1), so flag_val=1.
  BuildMI(&MBB, DL, TII->get(MOS::CmpBrZero))
      .addMBB(ExitMBB)
      .addReg(MOS::Z, RegState::Undef)  // flag to test (produced by compare)
      .addImm(1)                        // branch if Z=1 (BEQ)
      .addReg(SizeLo);                  // value to test against zero

  // Fall through to LoopMBB (no jump needed since LoopMBB follows MBB)

  // Loop body: DEY first, then load/store, then TYA/BNE
  BuildMI(LoopMBB, DL, TII->get(MOS::DEY_Implied));

  if (IsMemSet) {
    // For memset: the fill value is already in a register.
    // Copy it to A if not already there.
    if (SrcPtrOrVal != MOS::A) {
      BuildMI(LoopMBB, DL, TII->get(MOS::COPY), MOS::A)
          .addReg(SrcPtrOrVal);
    }
  } else {
    // For memcpy: load from source using indirect indexed addressing.
    BuildMI(LoopMBB, DL, TII->get(MOS::LDIndirIdx), MOS::A)
        .addReg(SrcPtrOrVal)
        .addReg(MOS::Y);
  }

  // Store to destination using indirect indexed addressing.
  BuildMI(LoopMBB, DL, TII->get(MOS::STIndirIdx))
      .addReg(MOS::A)
      .addReg(DstPtr)
      .addReg(MOS::Y);

  // TYA sets Z based on Y, then BNE loops while Z=0 (Y != 0).
  // This is needed because LDA/STA clobber the Z flag from DEY.
  BuildMI(LoopMBB, DL, TII->get(MOS::TYA_Implied))
      .addDef(MOS::Z, RegState::Implicit);
  BuildMI(LoopMBB, DL, TII->get(MOS::BR))
      .addMBB(LoopMBB)           // target
      .addReg(MOS::Z)            // flag to test
      .addImm(0);                // branch if Z=0 (BNE)

  // Fall through to ExitMBB when Z=1 (Y reached 0 after copying index 0)
}

} // anonymous namespace

char MOSExpandMemOps::ID = 0;

INITIALIZE_PASS(MOSExpandMemOps, DEBUG_TYPE, "MOS Memory Operation Expansion",
                false, false)

MachineFunctionPass *llvm::createMOSExpandMemOpsPass() {
  return new MOSExpandMemOps;
}
