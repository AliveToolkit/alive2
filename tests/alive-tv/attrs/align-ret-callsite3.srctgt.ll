; ERROR: Target is more poisonous than source

define i8* @src() {
  %p = call i8* @g()
  %p.fr = freeze i8* %p
  ret i8* %p.fr
}

define i8* @tgt() {
  %p = call i8* @g()
  ret i8* %p
}

declare align 4 i8* @g()
