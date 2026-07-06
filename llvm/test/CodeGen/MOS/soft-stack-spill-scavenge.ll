; RUN: llc -mtriple=mos-unknown-unknown -O2 < %s -o /dev/null
;
; A high-register-pressure reentrant function that spills heavily to the soft
; stack. The soft-stack spiller (see MOSInstrInfo::loadStoreRegStackSlot)
; needs a scratch Imag16 for STStk/LDStk expansion, and the register
; scavenger needs a save area for singleton-class registers A, Y, and P.
; If both roles share the same physical register, dense double-arithmetic
; blocks can drive the scavenger into a state where every candidate in a
; save class is unavailable and it aborts with
; "No register left to scavenge!" from PEI's frame-vreg scavenging.
;
; This test compiles a block modeled on that pattern (many volatile double
; loads and adds within one MachineBasicBlock) to ensure that the two roles
; remain separated. See MOSRegisterInfo::getReservedRegs.

target datalayout = "e-m:e-p:16:8-p1:8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-Fi8-n8"
target triple = "mos-unknown-unknown"

define noundef i16 @scavenge_20021120(float %0, float %1, float %2, double %3, double %4, double %5, double %add112.i, double %6, double %7, double %add110.i, double %8, double %add174.i, double %9, double %10, double %add108.i, double %11, double %add172.i, double %12, double %13, double %add106.i, double %14, double %add170.i, double %15, double %16, double %add104.i, double %17, double %add168.i, double %18, double %19, double %add102.i, double %20, double %add166.i, double %21, double %22, double %add100.i, double %23, double %add164.i, double %24, double %25, double %add98.i, double %26, double %add162.i, double %27, double %28, double %add96.i, double %29, double %add160.i, double %30, double %31, double %add94.i, double %32, double %add158.i, double %33, double %34, double %add156.i, double %35, double %36, double %add90.i, double %add154.i, double %add224.i, double %add226.i, double %add228.i, double %add230.i, double %add232.i, double %add234.i, double %add236.i, double %add238.i, double %add240.i, double %add242.i, double %add244.i) #0 {
entry:
  %37 = load volatile float, ptr null, align 1
  %38 = load volatile float, ptr null, align 1
  %39 = load volatile float, ptr null, align 1
  %40 = load volatile double, ptr null, align 8
  %41 = load volatile double, ptr null, align 8
  %42 = load volatile double, ptr null, align 8
  %43 = load volatile double, ptr null, align 8
  %44 = load volatile double, ptr null, align 8
  %45 = load volatile double, ptr null, align 8
  %46 = load volatile double, ptr null, align 8
  %47 = load volatile double, ptr null, align 8
  %48 = load volatile double, ptr null, align 8
  %49 = load volatile double, ptr null, align 8
  %50 = load volatile double, ptr null, align 8
  %51 = load volatile double, ptr null, align 8
  %52 = load volatile double, ptr null, align 8
  %53 = load volatile double, ptr null, align 8
  %54 = load volatile double, ptr null, align 8
  %55 = load volatile double, ptr null, align 8
  %56 = load volatile double, ptr null, align 8
  %57 = load volatile double, ptr null, align 8
  %58 = load volatile double, ptr null, align 8
  %59 = load volatile double, ptr null, align 8
  %60 = load volatile double, ptr null, align 8
  %61 = load volatile double, ptr null, align 8
  %62 = load volatile double, ptr null, align 8
  %63 = load volatile double, ptr null, align 8
  %64 = load volatile double, ptr null, align 8
  %65 = load volatile double, ptr null, align 8
  %66 = load volatile double, ptr null, align 8
  %67 = load volatile double, ptr null, align 8
  %68 = load volatile double, ptr null, align 8
  %69 = load volatile double, ptr null, align 8
  %70 = load volatile double, ptr null, align 8
  %71 = load volatile double, ptr null, align 8
  %72 = load volatile double, ptr null, align 8
  %73 = load volatile double, ptr null, align 8
  %74 = load volatile double, ptr null, align 8
  store volatile float %0, ptr null, align 1
  store volatile float %1, ptr null, align 1
  store volatile float %2, ptr null, align 1
  %add244.i1 = fadd double 0.000000e+00, %3
  %add242.i2 = fadd double 0.000000e+00, %4
  %add112.i3 = fadd double 0.000000e+00, %5
  %add240.i4 = fadd double %add112.i, %6
  %add110.i5 = fadd double 0.000000e+00, %7
  %add174.i6 = fadd double %add110.i, %8
  %add238.i7 = fadd double %add174.i, %9
  %add108.i8 = fadd double 0.000000e+00, %10
  %add172.i9 = fadd double %add108.i, %11
  %add236.i10 = fadd double %add172.i, %12
  %add106.i11 = fadd double 0.000000e+00, %13
  %add170.i12 = fadd double %add106.i, %14
  %add234.i13 = fadd double %add170.i, %15
  %add104.i14 = fadd double 0.000000e+00, %16
  %add168.i15 = fadd double %add104.i, %17
  %add232.i16 = fadd double %add168.i, %18
  %add102.i17 = fadd double 0.000000e+00, %19
  %add166.i18 = fadd double %add102.i, %20
  %add230.i19 = fadd double %add166.i, %21
  %add100.i20 = fadd double 0.000000e+00, %22
  %add164.i21 = fadd double %add100.i, %23
  %add228.i22 = fadd double %add164.i, %24
  %add98.i23 = fadd double 0.000000e+00, %25
  %add162.i24 = fadd double %add98.i, %26
  %add226.i25 = fadd double %add162.i, %27
  %add96.i26 = fadd double 0.000000e+00, %28
  %add160.i27 = fadd double %add96.i, %29
  %add224.i28 = fadd double %add160.i, %30
  %add94.i29 = fadd double 0.000000e+00, %31
  %add158.i30 = fadd double %add94.i, %32
  %add222.i = fadd double %add158.i, %33
  %add156.i31 = fadd double 0.000000e+00, %34
  %add220.i = fadd double %add156.i, %35
  %add90.i32 = fadd double 0.000000e+00, %36
  %add154.i33 = fadd double %add90.i, 1.000000e+00
  store volatile double %add154.i, ptr null, align 8
  store volatile double %add220.i, ptr null, align 8
  store volatile double %add222.i, ptr null, align 8
  store volatile double %add224.i, ptr null, align 8
  store volatile double %add226.i, ptr null, align 8
  store volatile double %add228.i, ptr null, align 8
  store volatile double %add230.i, ptr null, align 8
  store volatile double %add232.i, ptr null, align 8
  store volatile double %add234.i, ptr null, align 8
  store volatile double %add236.i, ptr null, align 8
  store volatile double %add238.i, ptr null, align 8
  store volatile double %add240.i, ptr null, align 8
  store volatile double %add242.i, ptr null, align 8
  store volatile double %add244.i, ptr null, align 8
  tail call void null() #1
  unreachable
}

attributes #0 = { noreturn nounwind optsize }
attributes #1 = { noreturn nounwind optsize }
