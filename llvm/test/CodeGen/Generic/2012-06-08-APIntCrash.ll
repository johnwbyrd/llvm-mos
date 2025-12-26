; RUN: llc < %s

; GlobalISel cannot legalize <8 x i32> vector operations on targets without
; native vector support. MOS fails at G_STORE legalization, X86 GlobalISel
; fails at G_BUILD_VECTOR. X86 SelectionDAG handles this via scalarization.
; UNSUPPORTED: target=mos{{.*}}

define void @test1(ptr %ptr)
{
	%1 = load <8 x i32>, ptr %ptr, align 32
	%2 = and <8 x i32> %1, <i32 0, i32 0, i32 0, i32 -1, i32 0, i32 0, i32 0, i32 -1>
	store <8 x i32> %2, ptr %ptr, align 16
	ret void
}
