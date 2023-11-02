@a = dso_local global <3 x i16> undef, align 1
@b = dso_local global <3 x i16> undef, align 1
@c = dso_local global <3 x i16> undef, align 1

define void @vector_add() {
    %a = load <3 x i16>, ptr @a, align 1
    %b = load <3 x i16>, ptr @b, align 1
    %d = add <3 x i16> %a, %b
    store <3 x i16> %d, ptr @c, align 1
    ret void
}
