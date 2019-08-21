; RUN: llvm-mc -triple mos -show-encoding < %s | FileCheck %s


foo:
  ora   (R10, x)

;; CHECK: ora    (00, x)          ; encoding: [0x01]
