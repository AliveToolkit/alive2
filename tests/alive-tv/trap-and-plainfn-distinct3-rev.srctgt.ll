define void @src() {
; trap and any function should not be considered equivalent
  call void @plain_fn()
; NOTE: due to issue #375, using unreachable here will unexpectedly make this test pass, because since plain_fn always returns.
  ret void
}

define void @tgt() {
  call void @llvm.trap()
  ret void
}

declare void @plain_fn()
declare void @llvm.trap()

; ERROR: Source is more defined than target
