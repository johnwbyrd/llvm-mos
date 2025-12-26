; RUN: llc -no-integrated-as < %s

; MOS uses GlobalISel exclusively, which doesn't support aggregate (struct)
; operands in inline asm. The constraint "=*m" with elementtype(%struct..0anon)
; passes an aggregate type. GlobalISel's InlineAsmLowering returns false at
; "Aggregate input operands are not supported yet" (line ~264). X86 with
; GlobalISel also fails; X86 SelectionDAG handles this. MOS has no SelectionDAG
; fallback. See llvm/lib/CodeGen/GlobalISel/InlineAsmLowering.cpp.
; UNSUPPORTED: target=mos{{.*}}

        %struct..0anon = type { [100 x i32] }

define void @test() {
entry:
        %currfpu = alloca %struct..0anon, align 16              ; <ptr> [#uses=2]
        %mxcsr = alloca %struct..0anon, align 16                ; <ptr> [#uses=1]
        call void asm sideeffect "fnstenv $0", "=*m,~{dirflag},~{fpsr},~{flags}"( ptr elementtype( %struct..0anon) %currfpu )
        call void asm sideeffect "$0  $1", "=*m,*m,~{dirflag},~{fpsr},~{flags}"( ptr elementtype( %struct..0anon) %mxcsr, ptr elementtype(%struct..0anon) %currfpu )
        ret void
}

