; RUN: llc < %s -fast-isel
; RUN: llc < %s

; NVPTX failed to lower i670010, as size > 64
; UNSUPPORTED: target=nvptx{{.*}}

; MOS uses GlobalISel exclusively and doesn't support FastISel. The -fast-isel
; flag causes "target does not support generation of this file type" error.
; UNSUPPORTED: target=mos{{.*}}

define i60 @PR24662b() {
  %1 = fptoui float 0x400D9999A0000000 to i670010
  %2 = trunc i670010 %1 to i60
  ret i60 %2
}
