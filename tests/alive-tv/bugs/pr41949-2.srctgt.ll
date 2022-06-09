; https://bugs.llvm.org/show_bug.cgi?id=41949

;source_filename = "41949.ll"
target datalayout = "E"

define void @src(i32* %p) {
  %u = alloca i32
  %a = bitcast i32* %u to i32*
  %b = bitcast i32* %u to i12*
  store i32 -1, i32* %a
  store i12 20, i12* %b
  call void @test1(i32* %u)
  ret void
}

define void @tgt(i32* %p) {
  %u = alloca i32
  %a = bitcast i32* %u to i32*
  store i32 22020095, i32* %a
  call void @test1(i32* %u)
  ret void
}

declare void @test1(i32*)

; ERROR: Source is more defined than target
