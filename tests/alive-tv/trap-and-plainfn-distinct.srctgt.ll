define void @src() {
  call void @llvm.trap()
  unreachable
}

define void @tgt() {
; trap and any readnone noreturn function should not be considered equivalent
  call void @plain_fn() noreturn memory(none)
  unreachable
}

declare void @plain_fn()
declare void @llvm.trap()

; ERROR: Source is more defined than target
