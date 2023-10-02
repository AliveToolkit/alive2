@a = dso_local global <9 x i15> undef, align 1
@b = dso_local global <9 x i15> undef, align 1
@c = dso_local global <9 x i15> undef, align 1

define void @vector_add() {
    %a = load <9 x i15>, ptr @a, align 1
    %b = load <9 x i15>, ptr @b, align 1
    %d = add <9 x i15> %a, %b
    store <9 x i15> %d, ptr @c, align 1
    ret void
}
