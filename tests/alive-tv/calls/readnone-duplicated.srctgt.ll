declare void @g()

define void @src(i1 %cmp) {
entry:
  %call = call i32* @__errno_location()
  call void @g()
  %call2 = call i32* @__errno_location()
  store i32 0, i32* %call2, align 4
  ret void
}

define void @tgt(i1 %cmp) {
entry:
  %call = call i32* @__errno_location()
  call void @g()
  store i32 0, i32* %call, align 4
  ret void
}

declare i32* @__errno_location() readnone
