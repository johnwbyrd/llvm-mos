; RUN: llc < %s

; GlobalISel cannot legalize <8 x float> vector operations. MOS fails at
; G_STORE, X86 GlobalISel fails at G_FADD for this vector type.
; UNSUPPORTED: target=mos{{.*}}

%f8 = type <8 x float>

define void @test_f8(ptr %P, ptr %Q, ptr %S) {
  %p = load %f8, ptr %P
  %q = load %f8, ptr %Q
  %R = fadd %f8 %p, %q
  store %f8 %R, ptr %S
  ret void
}

