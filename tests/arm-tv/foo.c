int foo(int x, int y){
    if (x>0)
        return x+1+y;
    if (y<0)
        return y-x-1;
    return x*y*3;
}