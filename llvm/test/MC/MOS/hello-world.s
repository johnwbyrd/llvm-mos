; RUN: llvm-mc -triple mos --filetype=obj -o=%t.obj %s 
; IGNORE: llvm-objdump --all-headers --print-imm-hex -D %t.obj 
; IGNORE: lld -flavor gnu %t.obj -o %t.elf
; IGNORE: llvm-readelf --all %t.elf 
; IGNORE: llvm-objcopy --output-target binary --strip-unneeded %t.elf %t.bin
_start:
	ldx	#$0                     ; CHECK: encoding: [0xa2,0x00]
loop:
	lda	hello,x                 ; CHECK: encoding: [0xb5,A]
                                ; CHECK:  fixup A - offset: 1, value: hello, kind: Addr8
	beq	done                    ; CHECK: encoding: [0xf0,A]
                                ; CHECK:  fixup A - offset: 1, value: done, kind: PCRel8
	jsr	$ffd2                   ; CHECK: encoding: [0x20,0xd2,0xff]
	inx                         ; CHECK: encoding: [0xe8]
	bne	loop                    ; CHECK: encoding: [0xd0,A]
                                ; CHECK:  fixup A - offset: 1, value: loop, kind: PCRel8
done:
	rts                             ; CHECK: encoding: [0x60]
hello:  
    .asciz "HELLO, WORLD!"