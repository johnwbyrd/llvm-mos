; RUN: llvm-mc -triple mos -mcpu=mos-6502 -show-encoding < %s | FileCheck %s
foo:
  lda #255
  lda 255
  lda 256
  lda 65535
;; CHECK: lda   5         ; encoding: [0x01 0x05]
