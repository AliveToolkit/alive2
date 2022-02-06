; This is a valid transformation, but alive-tv reports it as incorrect due to issue 650.
; SKIP-IDENTITY

define void @src() {
  %p = alloca i32
  store i32 0, i32* %p
  call void @writeonly(i32* %p)
  ret void
}

define void @tgt() {
  %p = alloca i32
  call void @writeonly(i32* %p)
  ret void
}

declare void @writeonly(i32*) writeonly

; ERROR: Source is more defined than target
