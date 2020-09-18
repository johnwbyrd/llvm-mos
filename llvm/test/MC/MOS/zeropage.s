; RUN: llvm-mc -assemble --print-imm-hex --show-encoding -triple mos < %s | FileCheck %s
 
adrAbsZ = 0xea
adrAbsA = 0xeaea

  .text

adrNearA:

  lda adrAbsZ                             ; encoding: [0xa5,0xea]
  lda adrAbsA                             ; encoding: [0xad,0xea,0xea]
  lda adrNearA                            ; encoding: [0xad,0x00,0x00]
                                          ;   fixup A - offset: 1, value: adrNearA, kind: Addr16
  lda adrFarZ1                            ; encoding: [0xa5,0x00]
                                          ;   fixup A - offset: 1, value: adrFarZ1, kind: Addr8
  lda adrFarZ2                            ; encoding: [0xa5,0x00]
                                          ;   fixup A - offset: 1, value: adrFarZ2, kind: Addr8
  lda adrFarA                             ; encoding: [0xad,0x00,0x00]
                                          ;   fixup A - offset: 1, value: adrFarA, kind: Addr16

  .section .zp,"",@nobits

adrFarZ1: .ds.b 1

  .section customzp,"z",@nobits

adrFarZ2: .ds.b 1

  .section nzp,"",@nobits

adrFarA: .ds.b 1
