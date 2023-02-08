        .text
        .file   "foo.c"
        .globl  foo
        .p2align        2
        .type   foo,@function
foo:
        .cfi_startproc
        adrp    x8, :got:x
        ldr     x8, [x8, :got_lo12:x]
        ldr     x9, [x8]
        add     x9, x9, #2
        str     x9, [x8]
        ret
.Lfunc_end0:
        .size   foo, .Lfunc_end0-foo
        .cfi_endproc

        .type   x,@object
        .bss
        .globl  x
        .p2align        3, 0x0
x:
        .xword  0
        .size   x, 8

        .ident  "clang version 17.0.0 (git@github.com:llvm/llvm-project.git c32022ad260aa1e6f485c5a6820fa9973f3b108e)"
        .section        ".note.GNU-stack","",@progbits
