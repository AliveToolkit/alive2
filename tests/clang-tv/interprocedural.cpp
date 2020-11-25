// TEST-ARGS: -O3
// A simple test that checks whether InferFunctionAttrsPass is skipped and
// SimplifyCFGPass is not skipped.

int f(){ return 0; }

// CHECK: InferFunctionAttrsPass : Skipping
// CHECK-NOT: SimplifyCFGPass : Skipping
