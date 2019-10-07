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

try 0 '0;'
try 42 '42;'
try 41 ' 12 + 34 - 5 ; '
try 47 '5+6*7;'
try 15 '5*(9-6);'
try 4 '(3+5)/2;'
try 5 '+5;'
try 2 '-1+3;'

try 0 '0==1;'
try 1 '42==42;'
try 1 '0!=1;'
try 0 '42!=42;'
try 1 '0<1;'
try 0 '1<1;'
try 0 '2<1;'
try 1 '0<=1;'
try 1 '1<=1;'
try 0 '2<=1;'
try 1 '1>0;'
try 0 '1>1;'
try 0 '1>2;'
try 1 '1>=0;'
try 1 '1>=1;'
try 0 '1>=2;'
try 42 '1;42;'
try 37 'a=42;b=a-5;b;'
try 37 'foo=42;bar=foo-5;bar;'
try 42 'return 42;'
try 4 'a=1; while (a<4) a=a+1; return a;'
try 2 'a=1; if (a==1) return 2; return 3;'
try 3 'a=4; if (a==1) return 2; return 3;'
try 2 'a=1; if (a==1) return 2; else return 3;'
try 3 'a=4; if (a==1) return 2; else return 3;'
try 5 'a=8; b=0; for (a=1; a<6; a=a+1) b=b+1; return b;'
try 42 '{a=42; return a;}'
try 8 'a=1;b=1; while (a<4) {a=a+1; b=b+1;} return a+b;'
try 5 'a=1; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;'
try 9 'a=6; b=1; if (a==1) {a=2;b=3;} else {a=4;b=5;} return a+b;'
try 0 'f(); return 0;'

echo OK
