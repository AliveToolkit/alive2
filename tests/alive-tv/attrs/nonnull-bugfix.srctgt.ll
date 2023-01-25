define void @src(ptr %db, ptr %p) {
entry:
  %0 = ptrtoint ptr %p to i8
  %1 = getelementptr i8, ptr %db, i32 0
  %2 = load i8, ptr %1, align 8 ; It was loaded, so %db cannot be null
  %call6 = call ptr @g(ptr %db)
  ret void
}

define void @tgt(ptr %db, ptr %p) {
entry:
  %0 = ptrtoint ptr %p to i8
  %1 = getelementptr i8, ptr %db, i32 0
  %2 = load i8, ptr %1, align 8
  %call6 = call ptr @g(ptr nonnull %db)
  ret void
}

declare ptr @g(ptr)
