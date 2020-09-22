; https://bugs.llvm.org/show_bug.cgi?id=1107

define i1 @src(i8 %A, i8 %B) {
        %a = zext i8 %A to i32          ; <i32> [#uses=1]
        %b = zext i8 %B to i32          ; <i32> [#uses=1]
        %c = icmp sgt i32 %a, %b                ; <bool> [#uses=1]
        ret i1 %c
}

define i1 @tgt(i8 %A, i8 %B) {
        %c = icmp sgt i8 %A, %B         ; <bool> [#uses=1]
        ret i1 %c
}

; ERROR: Target's return value is more undefined
