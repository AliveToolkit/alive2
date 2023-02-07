        .text
        .file   "foo.c"
        .globl  f
        .p2align        2
        .type   f,@function
f:
        .cfi_startproc
        adrp    x8, :got:s
        ldr     x8, [x8, :got_lo12:s]
        ldrsb   w9, [x8, #2]
        ldrh    w10, [x8, #22]
        ldrh    w11, [x8, #16]
        add     w10, w10, w0
        add     w10, w10, w11
        ldrh    w11, [x8, #8]
        ldrb    w12, [x8, #20]
        add     w11, w11, w12
        add     w11, w10, w11
        and     x13, x11, #0xffff
        str     x13, [x8, #8]
        add     w9, w11, w9
        add     w9, w9, w10, sxtb
        strb    w9, [x8, #2]
        ret
.Lfunc_end0:
        .size   f, .Lfunc_end0-f
        .cfi_endproc

        .type   s,@object
        .comm   s,24,8
        .ident  "Apple clang version 14.0.0 (clang-1400.0.29.202)"
        .section        ".note.GNU-stack","",@progbits
