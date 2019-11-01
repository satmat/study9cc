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

try 0 'main(){return 0;}'
try 42 'main(){return 42;}'
try 41 ' main(){return 12 + 34 - 5 ;} '
try 47 'main(){return 5+6*7;}'
try 15 'main(){return 5*(9-6);}'
try 4 'main(){return (3+5)/2;}'
try 5 'main(){return +5;}'
try 2 'main(){return -1+3;}'
try 0 'main(){return 0==1;}'
try 1 'main(){return 42==42;}'
try 1 'main(){return 0!=1;}'
try 0 'main(){return 42!=42;}'
try 1 'main(){return 0<1;}'
try 0 'main(){return 1<1;}'
try 0 'main(){return 2<1;}'
try 1 'main(){return 0<=1;}'
try 1 'main(){return 1<=1;}'
try 0 'main(){return 2<=1;}'
try 1 'main(){return 1>0;}'
try 0 'main(){return 1>1;}'
try 0 'main(){return 1>2;}'
try 1 'main(){return 1>=0;}'
try 1 'main(){return 1>=1;}'
try 0 'main(){return 1>=2;}'
try 1 'main(){return 1;42;}'
try 39 'main(){int a; int b; int c; a=42;b=3;c=a-b;return c;}'
try 37 'main(){int foo; int bar;foo=42;bar=foo-5;return bar;}'
try 4 'main(){int a; a=1; while (a<4) a=a+1; return a;}'
try 2 'main(){int a; a=1; if (a==1) return 2; return 3;}'
try 3 'main(){int a; a=4; if (a==1) return 2; return 3;}'
try 2 'main(){int a; a=1; if (a==1) return 2; else return 3;}'
try 3 'main(){int a; a=4; if (a==1) return 2; else return 3;}'
try 5 'main(){int a; int b; b=0; for (a=1; a<6; a=a+1) b=b+1; return b;}'
try 6 'main(){int a; int b; a=1;b=1; while (a<3) {a=a+1; b=b+1;} return a+b;}'
try 5 'main(){int a; int b; a=1; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;}'
try 9 'main(){int a; int b; a=6; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;}'
try 44 'foo(){return 42;} main(){return foo()+2;}'
try 39 'foo(int a){return a+5;} main(){return foo(34);}'
try 42 'foo(int a, int b){return a+b;} main(){int c; c=foo(40,2); return c;}'
try 2 'foo(int a, int b){return b;} main(){int c; c=foo(40,2); return c;}'
try 11 'foo(int a,int b,int c,int d,int e,int f){return f;} main(){int c; c=foo(1,3,5,7,9,11); return c;}'

echo OK
