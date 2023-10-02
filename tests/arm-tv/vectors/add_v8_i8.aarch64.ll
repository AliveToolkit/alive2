@a = dso_local global i64 0, align 1
@b = dso_local global i64 0, align 1
@c = dso_local global i64 0, align 1

define void @square() {
    %a = load <8 x i8>, ptr @a, align 1
    %b = load <8 x i8>, ptr @b, align 1
    %d = add <8 x i8> %a, %b
    store <8 x i8> %d, ptr @c, align 1
    ret void
}
