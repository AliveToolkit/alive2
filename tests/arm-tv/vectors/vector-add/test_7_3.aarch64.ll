@a = dso_local global <7 x i3> undef, align 1
@b = dso_local global <7 x i3> undef, align 1
@c = dso_local global <7 x i3> undef, align 1

define void @vector_add() {
    %a = load <7 x i3>, ptr @a, align 1
    %b = load <7 x i3>, ptr @b, align 1
    %d = add <7 x i3> %a, %b
    store <7 x i3> %d, ptr @c, align 1
    ret void
}
