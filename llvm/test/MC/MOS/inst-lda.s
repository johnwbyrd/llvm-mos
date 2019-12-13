; RUN: llvm-mc -triple mos -mcpu=mos-6502 -show-encoding < %s | FileCheck %s

foo:
  lda #255
  lda 0xff
  lda 257
  lda 65535
  lda #0

;; CHECK: lda #255                        ; encoding: [0xa9,0xff]
;; CHECK: lda 255                         ; encoding: [0xa5,0xff]
;; CHECK: lda 257                         ; encoding: [0xad,0x01,0x01]
;; CHECK: lda 65535                       ; encoding: [0xad,0xff,0xff]
;; CHECK: lda #0                          ; encoding: [0xa9,0x00]
