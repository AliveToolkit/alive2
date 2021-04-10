define void @src(<2 x i32> %input) { ; <1, 0>
    %cond = icmp eq <2 x i32> %input, zeroinitializer
    %.14 = extractelement <2 x i32> %input, i32 1
    %.15 = insertelement <2 x i32> poison, i32 %.14, i32 0
    %.16 = extractelement <2 x i32> %input, i32 0
    %.17 = insertelement <2 x i32> %.15, i32 %.16, i32 1
    %.18 = select <2 x i1> %cond, <2 x i32> %.17, <2 x i32> %input
    %x = extractelement <2 x i32> %.18, i32 0
    %y = extractelement <2 x i32> %.18, i32 1
    call void @f(i32 %x, i32 %y)
    ret void
}

define void @tgt(<2 x i32> %input) { ; <1, 0>
  %x = extractelement <2 x i32> %input, i32 0
  %y = extractelement <2 x i32> %input, i32 1
  call void @f(i32 %x, i32 %y)
  ret void
}

declare void @f(i32, i32)

; ERROR: Source is more defined than target
