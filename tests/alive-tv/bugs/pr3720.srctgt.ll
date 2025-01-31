define void @src(ptr %p) {
        %s = alloca i16, align 4
        store i16 1, ptr %s
        call void @llvm.memcpy.i32(ptr %p, ptr %s, i32 2, i1 false)
        ret void
}

declare void @llvm.memcpy.i32(ptr captures(none), ptr captures(none), i32, i1)

define void @tgt(ptr %p) {
        store i16 1, ptr %p
        ret void
}

; ERROR: Source is more defined than target
