; Found by Alive2

define void @src(i32 %a, i32 %b) {
  %rem = srem i32 %a, %b
  %div = sdiv i32 %a, %b
  call void @foo(i32 %rem, i32 %div)
  ret void
}
define void @tgt(i32 %a, i32 %b) {
  %div = sdiv i32 %a, %b
  %1 = mul i32 %div, %b
  %2 = sub i32 %a, %1
  call void @foo(i32 %2, i32 %div)
  ret void
}

declare void @foo(i32, i32)

; ERROR: Source is more defined than target
