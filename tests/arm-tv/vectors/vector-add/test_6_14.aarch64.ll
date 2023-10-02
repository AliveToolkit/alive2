@a = dso_local global <6 x i14> undef, align 1
@b = dso_local global <6 x i14> undef, align 1
@c = dso_local global <6 x i14> undef, align 1

define void @vector_add() {
    %a = load <6 x i14>, ptr @a, align 1
    %b = load <6 x i14>, ptr @b, align 1
    %d = add <6 x i14> %a, %b
    store <6 x i14> %d, ptr @c, align 1
    ret void
}
