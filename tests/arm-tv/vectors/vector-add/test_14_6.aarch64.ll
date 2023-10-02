@a = dso_local global <14 x i6> undef, align 1
@b = dso_local global <14 x i6> undef, align 1
@c = dso_local global <14 x i6> undef, align 1

define void @vector_add() {
    %a = load <14 x i6>, ptr @a, align 1
    %b = load <14 x i6>, ptr @b, align 1
    %d = add <14 x i6> %a, %b
    store <14 x i6> %d, ptr @c, align 1
    ret void
}
