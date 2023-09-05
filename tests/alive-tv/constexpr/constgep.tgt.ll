@x = constant i16 0
@y = constant ptr getelementptr inbounds (i8, ptr @x, i64 0)

define ptr @f() {
  %p = load ptr, ptr @y ; to relieve mismatch in memory error
  %b = getelementptr inbounds i8, ptr @x, i64 0
  ret ptr %b
}
