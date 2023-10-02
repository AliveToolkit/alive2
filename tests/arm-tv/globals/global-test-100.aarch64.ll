@a = dso_local global i64 0, align 1
@b = dso_local global i64 0, align 1
@c = dso_local global i64 0, align 1
define void @square() {
  %a = load i64, ptr @a, align 1
  %b = load i64, ptr @b, align 1
  %d = add i64 %a, %b
  store i64 %d, ptr @c, align 1
  ret void
}