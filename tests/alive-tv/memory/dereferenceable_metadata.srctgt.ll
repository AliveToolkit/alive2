define void @src(ptr %0) {
  load <6 x i16>, ptr %0, align 16, !dereferenceable !0
  ret void
}

define void @tgt(ptr %0) {
  load i128, ptr %0, align 16
  ret void
}

!0 = !{ i32 16 }
