; RUN: llc < %s

; GlobalISel cannot legalize <2 x double> vector operations. MOS fails on
; G_PHI, X86 GlobalISel fails on G_SELECT for this vector type.
; UNSUPPORTED: target=mos{{.*}}

define <2 x double> @vector_select(<2 x double> %x, <2 x double> %y) nounwind  {
	%x.lo = extractelement <2 x double> %x, i32 0		; <double> [#uses=1]
	%x.lo.ge = fcmp oge double %x.lo, 0.000000e+00		; <i1> [#uses=1]
	%a.d = select i1 %x.lo.ge, <2 x double> %y, <2 x double> %x		; <<2 x double>> [#uses=1]
	ret <2 x double> %a.d
}
