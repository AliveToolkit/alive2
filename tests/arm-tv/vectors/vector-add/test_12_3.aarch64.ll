@a = dso_local global <12 x i3> undef, align 1
@b = dso_local global <12 x i3> undef, align 1
@c = dso_local global <12 x i3> undef, align 1

define void @vector_add() {
    %a = load <12 x i3>, ptr @a, align 1
    %b = load <12 x i3>, ptr @b, align 1
    %d = add <12 x i3> %a, %b
    store <12 x i3> %d, ptr @c, align 1
    ret void
}
