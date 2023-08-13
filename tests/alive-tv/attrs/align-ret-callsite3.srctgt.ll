; ERROR: Target is more poisonous than source

define ptr @src() {
  %p = call ptr @g()
  %p.fr = freeze ptr %p
  ret ptr %p.fr
}

define ptr @tgt() {
  %p = call ptr @g()
  ret ptr %p
}

declare align 4 ptr @g()
