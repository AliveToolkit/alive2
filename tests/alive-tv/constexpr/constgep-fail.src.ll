@x = constant i16 0
@y = constant ptr getelementptr inbounds (i8, ptr @x, i64 0)

define ptr @f() {
  %p = load ptr, ptr @y
  ret ptr %p
}

; ERROR: Value mismatch
