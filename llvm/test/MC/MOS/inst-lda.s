; RUN: llvm-mc -triple mos -mcpu=mos-6502 -show-encoding < %s | FileCheck %s
foo:
  lda #255
  lda 255
  lda 256
  lda 257
  lda 258,x
  lda 259,y
  lda foo
  lda 65535
  lda #0
  beq foo
;; CHECK: lda   5         ; encoding: [0x01 0x05]
