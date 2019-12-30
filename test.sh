#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 0 'int main(){return 0;}'
try 42 'int main(){return 42;}'
try 41 ' int main(){return 12 + 34 - 5 ;} '
try 47 'int main(){return 5+6*7;}'
try 15 'int main(){return 5*(9-6);}'
try 4 'int main(){return (3+5)/2;}'
try 5 'int main(){return +5;}'
try 2 'int main(){return -1+3;}'
try 0 'int main(){return 0==1;}'
try 1 'int main(){return 42==42;}'
try 1 'int main(){return 0!=1;}'
try 0 'int main(){return 42!=42;}'
try 1 'int main(){return 0<1;}'
try 0 'int main(){return 1<1;}'
try 0 'int main(){return 2<1;}'
try 1 'int main(){return 0<=1;}'
try 1 'int main(){return 1<=1;}'
try 0 'int main(){return 2<=1;}'
try 1 'int main(){return 1>0;}'
try 0 'int main(){return 1>1;}'
try 0 'int main(){return 1>2;}'
try 1 'int main(){return 1>=0;}'
try 1 'int main(){return 1>=1;}'
try 0 'int main(){return 1>=2;}'
try 1 'int main(){return 1;42;}'
try 39 'int main(){int a; int b; int c; a=42;b=3;c=a-b;return c;}'
try 37 'int main(){int foo; int bar;foo=42;bar=foo-5;return bar;}'
try 4 'int main(){int a; a=1; while (a<4) a=a+1; return a;}'
try 2 'int main(){int a; a=1; if (a==1) return 2; return 3;}'
try 3 'int main(){int a; a=4; if (a==1) return 2; return 3;}'
try 2 'int main(){int a; a=1; if (a==1) return 2; else return 3;}'
try 3 'int main(){int a; a=4; if (a==1) return 2; else return 3;}'
try 5 'int main(){int a; int b; b=0; for (a=1; a<6; a=a+1) b=b+1; return b;}'
try 6 'int main(){int a; int b; a=1;b=1; while (a<3) {a=a+1; b=b+1;} return a+b;}'
try 5 'int main(){int a; int b; a=1; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;}'
try 9 'int main(){int a; int b; a=6; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;}'
try 44 'int foo(){return 42;} int main(){return foo()+2;}'
try 39 'int foo(int a){return a+5;} int main(){return foo(34);}'
try 42 'int foo(int a, int b){return a+b;} int main(){int c; c=foo(40,2); return c;}'
try 2 'int foo(int a, int b){return b;} int main(){int c; c=foo(40,2); return c;}'
try 11 'int foo(int a,int b,int c,int d,int e,int f){return f;} int main(){int c; c=foo(1,3,5,7,9,11); return c;}'
try 2 'int main(){int a; a = 1; int *b; b = &a; *b = 2; return a;}'
try 8 'int main(){int a; int *p; p = &a; p = p + 2; return p - &a;}'
try 12 'int main(){int b; int *q; q = &b; q = q - 3; return &b - q;}'
try 4 'int main(){int a; int b; a = 5; b = 2; int *p; int *q; p = &a; q = &b; return q - p;}'  # ローカル変数の定義順とメモリが確保される順が逆?
try 4 'int main(){return sizeof(int);}'
try 4 'int main(){int x; return sizeof(x);}'
try 8 'int main(){int *x; return sizeof(x);}'
try 4 'int main(){int *x; return sizeof(*x);}'
try 4 'int main(){return sizeof(1);}'
try 4 'int main(){return sizeof(sizeof(1));}'
try 40 'int main(){int a[10]; return sizeof(a);}'
try 3 'int main(){int a[2]; *a=1; *(a+1)=2; int* p; p=a; return *p+*(p+1);}'
try 3 'int main(){int a[2]; a[0]=1; a[1]=2; int* p; p=a; return *p+*(p+1);}'
try 1 'int g; int main(){g=1; return g;}'

echo OK
