; RUN: llvm-mc -triple mos -mcpu=mos-generic -show-encoding < %s | FileCheck %s
; source: https://michaelcmartin.github.io/Ophis/book/x162.html

ldx	#$0                     ; CHECK: encoding: [0xa2,0x00]
loop:   lda hello, x
        beq done
        jsr $ffd2
        inx
        bne loop
done:   rts
hello:  .byte "HELLO, WORLD!", 0