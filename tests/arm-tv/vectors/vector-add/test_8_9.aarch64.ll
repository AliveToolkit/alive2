@a = dso_local global <8 x i9> undef, align 1
@b = dso_local global <8 x i9> undef, align 1
@c = dso_local global <8 x i9> undef, align 1

define void @vector_add() {
    %a = load <8 x i9>, ptr @a, align 1
    %b = load <8 x i9>, ptr @b, align 1
    %d = add <8 x i9> %a, %b
    store <8 x i9> %d, ptr @c, align 1
    ret void
}
