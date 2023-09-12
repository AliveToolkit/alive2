@a = constant i32 0
@x = constant { ptr, ptr } { ptr @a , ptr @a }

define ptr @f() {
  ret ptr @a
}
