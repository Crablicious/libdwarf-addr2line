/*  Test file.
    Copyright (c) 2021 David Anderson
    This test file is hereby placed in the public domain
    for anyone to use for any purpose. */

/* compiled with
    cc -g -O2 test.c -o test
    or
    cc -gdwarf-5 -O2 test.c -o dwarf5test
*/


static int
foo(int y)
{
    int v= 32;
    int x= 12;
    int res = 0;

    res = v+x+y;
    return res;
}
int main()
{
    float v = 13.2;
    int top = 3;
    float sum = 0.0;
    sum = v +foo(9);
    sum += top;
    return 0; 
}
