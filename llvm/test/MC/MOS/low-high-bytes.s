; RUN: llvm-mc -triple mos --filetype=obj -o=%t.obj %s 
; RUN: llvm-objdump --all-headers --print-imm-hex -D %t.obj 
; RUN: lld -flavor gnu %t.obj -o %t.elf
; RUN: llvm-readelf --all %t.elf 
; RUN: llvm-objcopy --output-target binary --strip-unneeded %t.elf %t.bin
; RUN: fail

chrout = $ffd0 + (2 * 2) - 2

_start:
	ldx	#mos16lo(chrout)                     ; CHECK: encoding: [0xa2,0x00]
    stx $10
    lda #mos16hi(chrout)
    sta $11

done:
	rts                             ; CHECK: encoding: [0x60]
