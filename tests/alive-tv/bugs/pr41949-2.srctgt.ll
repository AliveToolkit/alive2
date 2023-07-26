; https://bugs.llvm.org/show_bug.cgi?id=41949
; To detect a bug from this, bytes of escaped local blocks should be checked

;source_filename = "41949.ll"
target datalayout = "E"

define void @src(ptr %p) {
  %u = alloca i32
  %a = bitcast ptr %u to ptr
  %b = bitcast ptr %u to ptr
  store i32 -1, ptr %a
  store i12 20, ptr %b
  call void @test1(ptr %u)
  ret void
}

define void @tgt(ptr %p) {
  %u = alloca i32
  %a = bitcast ptr %u to ptr
  store i32 22020095, ptr %a
  call void @test1(ptr %u)
  ret void
}

declare void @test1(ptr)
