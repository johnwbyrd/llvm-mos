; RUN: llc < %s

; PR1239

; Undefined external symbol "__powisf2"
; XFAIL: target=nvptx{{.*}}

; GlobalISel cannot legalize G_FPOWI (floating-point power with integer exp).
; Both MOS and X86 GlobalISel fail. X86 SelectionDAG handles this.
; UNSUPPORTED: target=mos{{.*}}

define float @test(float %tmp23302331, i32 %tmp23282329 ) {

%tmp2339 = call float @llvm.powi.f32.i32( float %tmp23302331, i32 %tmp23282329 )
	ret float %tmp2339
}

declare float @llvm.powi.f32.i32(float,i32)
