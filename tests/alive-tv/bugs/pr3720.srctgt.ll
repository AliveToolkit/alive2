%struct.st = type <{ i16 }>

define void @src(ptr %p) {
entry:
        %s = alloca %struct.st, align 4
        %0 = getelementptr %struct.st, ptr %s, i32 0, i32 0
        store i16 1, ptr %0, align 4
        %s1 = bitcast ptr %s to ptr
        call void @llvm.memcpy.i32(ptr %p, ptr %s1, i32 2, i32 1)
        ret void
}

declare void @llvm.memcpy.i32(ptr nocapture, ptr nocapture, i32, i32)

define void @tgt(ptr %p) {
entry:
        %p1 = bitcast ptr %p to ptr
        %p1.0 = getelementptr %struct.st, ptr %p1, i32 0, i32 0
        store i16 1, ptr %p1.0
        ret void
}

; ERROR: Source is more defined than target
