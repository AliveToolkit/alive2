define <4 x i32> @fn(ptr %in) {
  %t17 = load <4 x ptr>, ptr %in
  %t18 = icmp eq <4 x ptr> %t17, zeroinitializer
  %t19 = zext <4 x i1> %t18 to <4 x i32>
  ret <4 x i32> %t19
}
