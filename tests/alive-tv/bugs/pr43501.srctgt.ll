; Found by Alive2

define [4 x float]* @src([9 x [4 x float]]* %A) {
  %c = bitcast [9 x [4 x float]]* %A to [4 x float]*
  ret [4 x float]* %c
}

define [4 x float]* @tgt([9 x [4 x float]]* %A) {
	%c = getelementptr inbounds [9 x [4 x float]], [9 x [4 x float]]* %A, i64 0, i64 0
	ret [4 x float]* %c
}

; ERROR: Target is more poisonous than source
