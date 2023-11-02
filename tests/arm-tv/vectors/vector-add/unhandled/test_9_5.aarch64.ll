@a = dso_local global <9 x i5> undef, align 1
@b = dso_local global <9 x i5> undef, align 1
@c = dso_local global <9 x i5> undef, align 1

define void @vector_add() {
    %a = load <9 x i5>, ptr @a, align 1
    %b = load <9 x i5>, ptr @b, align 1
    %d = add <9 x i5> %a, %b
    store <9 x i5> %d, ptr @c, align 1
    ret void
}
