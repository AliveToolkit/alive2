declare void @g()

define void @src() {
  %call = call ptr @__errno_location()
  call void @g()
  %call2 = call ptr @__errno_location()
  store i32 0, ptr %call2, align 4
  ret void
}

define void @tgt() {
  %call = call ptr @__errno_location()
  call void @g()
  store i32 0, ptr %call, align 4
  ret void
}

declare ptr @__errno_location() memory(none)
