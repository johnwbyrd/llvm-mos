; RUN: llc < %s | FileCheck %s

; GlobalISel doesn't support the 'invoke' instruction (exception handling).
; The f1 function uses invoke with a landing pad. X86 passes because it uses
; SelectionDAG by default. MOS uses GlobalISel exclusively with no fallback.
; See llvm/lib/CodeGen/GlobalISel/IRTranslator.cpp - visitInvoke returns false.
; UNSUPPORTED: target=mos{{.*}}

declare i32 @__gxx_personality_v0(...) addrspace(0)
declare void @__cxa_call_unexpected(ptr)
declare void @llvm.donothing() readnone

; CHECK: f1
define void @f1() nounwind uwtable ssp personality ptr @__gxx_personality_v0 {
entry:
; CHECK-NOT: donothing
  invoke void @llvm.donothing()
  to label %invoke.cont unwind label %lpad

invoke.cont:
  ret void

lpad:
  %0 = landingpad { ptr, i32 }
          filter [0 x ptr] zeroinitializer
  %1 = extractvalue { ptr, i32 } %0, 0
  tail call void @__cxa_call_unexpected(ptr %1) noreturn nounwind
  unreachable
}

; CHECK: f2
define void @f2() nounwind {
entry:
; CHECK-NOT: donothing
  call void @llvm.donothing()
  ret void
}
