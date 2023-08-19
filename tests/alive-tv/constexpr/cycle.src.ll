@g1 = constant ptr getelementptr inbounds (ptr, ptr @g2, i64 1)
@g2 = constant ptr getelementptr inbounds (ptr, ptr @g1, i64 1)

define ptr @f() {
  %x  = load ptr, ptr @g2
  ret ptr %x
}
