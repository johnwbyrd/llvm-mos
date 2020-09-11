  .text

  lda test1
  lda test2
  lda test3

  .section .zp

test1: .ds 1

  .section nzp

test2: .ds 1

  .section customzp, "z"

test3: .ds 1
