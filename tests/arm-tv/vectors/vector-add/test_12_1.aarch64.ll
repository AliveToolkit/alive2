@a = dso_local global <12 x i1> undef, align 1
@b = dso_local global <12 x i1> undef, align 1
@c = dso_local global <12 x i1> undef, align 1

define void @vector_add() {
    %a = load <12 x i1>, ptr @a, align 1
    %b = load <12 x i1>, ptr @b, align 1
    %d = add <12 x i1> %a, %b
    store <12 x i1> %d, ptr @c, align 1
    ret void
}
