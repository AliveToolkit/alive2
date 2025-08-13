; TEST-ARGS: -tgt-is-asm
; XFAIL: ERROR: Source is more defined than target
; https://github.com/AliveToolkit/alive2/issues/1222

define void @src(ptr %0, ptr %1) {
  store i16 0, ptr %0, align 4
  store i16 0, ptr %1, align 8
  ret void
}

define void @tgt(ptr %0, ptr %1) {
arm_tv_entry:
  store i16 0, ptr %1, align 8  
  store i16 0, ptr %0, align 4
  ret void
}