declare void @g()

define void @src() {
  %call = call i32* @__errno_location()
  call void @g()
  %call2 = call i32* @__errno_location()
  store i32 0, i32* %call2, align 4
  ret void
}

define void @tgt() {
  %call = call i32* @__errno_location()
  call void @g()
  store i32 0, i32* %call, align 4
  ret void
}

declare i32* @__errno_location() memory(none)
