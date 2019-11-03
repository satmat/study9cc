#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

// ローカル変数の型
typedef struct LVar LVar;

// ローカル変数の型
struct LVar {
  LVar *next; // 次の変数かNULL
  Type *ty;
  char *name;
  int offset; // RBPからのオフセット
};

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,      // +
  ND_SUB,      // -
  ND_MUL,      // *
  ND_DIV,      // /
  ND_ASSIGN,   // =
  ND_EQ,       // ==
  ND_NE,       // !=
  ND_LT,       // <
  ND_LE,       // <=
  ND_LVAR,     // ローカル変数
  ND_FUNCCALL, // 関数呼び出し
  ND_RETURN,   // return
  ND_IF,       // if
  ND_WHILE,    // while
  ND_ELSE,     // else
  ND_FOR,      // for
  ND_BLOCK,    // ブロック
  ND_NUM,      // 整数
  ND_ADDR,     // unary &
  ND_DEREF,    // unary *
  ND_NULL,     // Empty statement
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
  NodeKind kind;
  Type *ty;      // intやintへのポインタなどの型
  Node *lhs;
  Node *rhs;
  Node *init;
  Node *cond;
  Node *inc;
  Node *then;
  Node *els;
  Node *body;
  Node *next;

  // 変数
  LVar *var;

  // 整数リテラル
  int val;

  // 関数
  char *funcname;
  Node *args;
};

// トークンの種類
typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT,    // 識別子
  TK_RETURN,   // return
  TK_IF,       // if
  TK_ELSE,     // else
  TK_WHILE,    // while
  TK_FOR,      // for
  TK_INT,      // int
  TK_NUM,      // 整数トークン
  TK_EOF,      // 入力の終わりを表すトークン
} TokenKind;

// トークン型
typedef struct Token Token;

// トークン型
struct Token {
  TokenKind kind;  // トークンの型
  Token *next;     // 次のトークン
  int val;         // kindがTK_NUMの場合、その数値
  Type *ty;        // kindがTK_NUMの場合、その数値
  char *str;       // トークン文字列
  int len;         // トークンの長さ
};

// 現在着目しているトークン
extern Token *token;

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  LVar *params;

  Node *node;
  LVar *locals;
  int stack_size;
};

typedef struct {
  Function *fns;
} Program;

typedef enum {
  TY_INT,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;
  int align;
};

extern char *user_input;

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
bool consume(char *op);
Token *peek(char *s);
Token *consume_ident();
void expect(char *op);
int expect_number();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
int align_to(int n, int align);
bool startswith(char *p, char *q);
int is_alnum(char c);
void tokenize();
Node *new_node(NodeKind kind);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
Node *new_num(int val);
Program *program();
Function *function();
Node *assign();
Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();
void gen(Node *node);
void codegen(Program *prog);
void add_type(Node *node);

extern Type *int_type;
