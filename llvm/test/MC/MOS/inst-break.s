; RUN: llvm-mc -triple mos -mattr=break -show-encoding < %s | FileCheck %s


foo:

  brk

; CHECK: brk                  ; encoding: [0x00]
