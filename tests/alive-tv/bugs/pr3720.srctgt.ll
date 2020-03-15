%struct.st = type <{ i16 }>

define void @src(i8* %p) nounwind {
entry:
        %s = alloca %struct.st, align 4  ; <%struct.st*> [#uses=2]
        %0 = getelementptr %struct.st, %struct.st* %s, i32 0, i32 0  ; <i16*> [#uses=1]
        store i16 1, i16* %0, align 4
        %s1 = bitcast %struct.st* %s to i8*  ; <i8*> [#uses=1]
        call void @llvm.memcpy.i32(i8* %p, i8* %s1, i32 2, i32 1)
        ret void
}

declare void @llvm.memcpy.i32(i8* nocapture, i8* nocapture, i32, i32) nounwind

define void @tgt(i8* %p) nounwind {
entry:
        %p1 = bitcast i8* %p to %struct.st*  ; <%struct.st*> [#uses=1]
        %p1.0 = getelementptr %struct.st, %struct.st* %p1, i32 0, i32 0  ; <i16*> [#uses=1]
        store i16 1, i16* %p1.0
        ret void
}

; ERROR: Source is more defined than target
