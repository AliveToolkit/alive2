; TEST-ARGS: -tgt-is-asm

@bar = constant i32 6

define <4 x i32> @f(ptr %RP) {
   %LGV = load i32, ptr @bar, align 4
   ret <4 x i32> zeroinitializer
}
