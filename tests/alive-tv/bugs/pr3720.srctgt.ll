define void @src(ptr %p) {
        %s = alloca i16, align 4
        store i16 1, ptr %s
        call void @llvm.memcpy.i32(ptr %p, ptr %s, i32 2, i32 1)
        ret void
}

declare void @llvm.memcpy.i32(ptr nocapture, ptr nocapture, i32, i32)

define void @tgt(ptr %p) {
        store i16 1, ptr %p
        ret void
}

; ERROR: Source is more defined than target
