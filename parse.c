#include <string.h>
#include "9cc.h"

// 現在着目しているトークン
Token *token;

static LVar *locals;

// include string.hしても関数が見つからずにwarningになってしまうため
// 解決策が見つかるまで自前で定義
char *strndup(const char *s, size_t n) {
    char *p = memchr(s, '\0', n);
    if (p != NULL)
        n = p - s;
    p = malloc(n + 1);
    if (p != NULL) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, "");  // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next)
    if(strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
      return var;
  return NULL;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
  if ((token->kind != TK_RESERVED &&
      token->kind != TK_RETURN &&
      token->kind != TK_WHILE &&
      token->kind != TK_IF &&
      token->kind != TK_ELSE &&
      token->kind != TK_FOR ) ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// そのトークンを返す。それ以外の場合にはNULLを返す。
Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  token = token->next;
  return tok;
}

// 次のトークンが期待している記号ときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
  if (( token->kind != TK_RESERVED &&
        token->kind != TK_RETURN &&
        token->kind != TK_WHILE &&
        token->kind != TK_IF &&
        token->kind != TK_ELSE &&
        token->kind != TK_FOR ) ||
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error_at(token->str, "'%s'ではありません", op);
  token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// 識別子の文字列を返す。それ以外の場合にはNULLを返す。
char *expect_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tok = token;
  token = token->next;
  return strndup(tok->str, tok->len);
}

bool at_eof() {
  return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

LVar *new_var(char *name) {
  LVar *var = calloc(1, sizeof(LVar));
  var->name = name;
  return var;
}

LVar *new_lvar(char *name) {
  LVar *var = new_var(name);
  var->next = locals;
  locals = var;
  return var;
}

int align_to(int n, int align) {
  return (n + align - 1) & ~(align - 1);
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

int is_alpha(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         (c == '_');
}

int is_alnum(char c) {
  return is_alpha(c) ||
         ('0' <= c && c <= '9');
}

// 入力文字列pをトークナイズしてそれを返す
void tokenize() {
  Token head;
  head.next = NULL;
  Token *cur = &head;
  char *p = user_input;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "while", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (strncmp(p, "else", 4) == 0 && !is_alnum(p[4])) {
      cur = new_token(TK_ELSE, cur, p, 4);
      p += 4;
      continue;
    }

    if (strncmp(p, "for", 3) == 0 && !is_alnum(p[3])) {
      cur = new_token(TK_FOR, cur, p, 3);
      p += 3;
      continue;
    }

    if (strncmp(p, "if", 2) == 0 && !is_alnum(p[2])) {
      cur = new_token(TK_IF, cur, p, 2);
      p += 2;
      continue;
    }

    if(startswith(p, "==") || startswith(p, "!=") ||
       startswith(p, "<=") || startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (strchr("+-*/()<>{}=;,", *p) != NULL )
    {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (is_alpha(*p)) {
      char* q = p++;
      while(is_alnum(*p))
        p++;
      cur = new_token(TK_IDENT, cur, q, p-q);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char* q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error("トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  token = head.next;
  return;
}

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

// program = function*
Program *program(void) {
  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    Function *fn = function();
    if(!fn)
      continue;
    cur->next = fn;
    cur = cur->next;
    continue;
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->fns = head.next;
  return prog;
}

// declarator = ident
void declarator(char **name) {
  *name = expect_ident(&name);
}

// param = declarator
LVar *read_func_param(void) {
  char *name = NULL;
  declarator(&name);
  LVar *lv = new_lvar(name);
  return lv;
}

// params = param ("," param)*
void read_func_params(Function *fn) {
  if (consume(")"))
    return;

  Token *tok = token;

//  fn->params = read_func_param();
//  LVar *cur = fn->params;
  LVar *cur = read_func_param();

  while (!consume(")")) {
    expect(",");
    cur = read_func_param();
  }
  fn->params = cur;
}

// function = declarator "(" params? ")" ("{" stmt* "}" | ";")
Function *function(void) {
  locals = NULL;

  char *name = NULL;
  declarator(&name);

  // Construct a function object
  Function *fn = calloc(1, sizeof(Function));
  fn->name = name;
  expect("(");
  read_func_params(fn);

  if (consume(";")) {
    return NULL;
  }

  // Read function body
  Node head = {};
  Node *cur = &head;
  expect("{");
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// stmt = expr ";"
//        | "return" expr ";"
//        | "while" "(" expr ")" stmt
//        | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "{" stmt* "}"
Node *stmt() {
  Node *node;

  if (consume("return")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->rhs = expr();

    if (!consume(";"))
      error_at(token->str, "';'ではないトークンです");
    return node;
  } else if (consume("while")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  } else if (consume("for")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    expect("(");
    if(!consume(";")) {
      node->init = expr();
      expect(";");
    }
    if(!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if(!consume(";")) {
      node->inc = expr();
    }
    expect(")");
    node->then = stmt();
    return node;
  } else if (consume("if")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if(consume("else")) {
      node->els = stmt();
    }
    return node;
  } else if (consume("{")) {
    Node head = {};
    Node *cur = &head;

    while(!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }

    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = head.next;
    return node;
  } else {
    node = expr();

    if (!consume(";"))
      error_at(token->str, "';'ではないトークンです");
    return node;
  }
}

// expr = assign
Node *expr() {
  return assign();
}

// assign = equality ("=" assign)?
Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_binary(ND_ASSIGN, node, assign());
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for(;;) {
    if (consume("=="))
      node = new_binary(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_binary(ND_NE, node, relational());
    else
      return node;
  }
}

// add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_binary(ND_LT, node, add());
    else if (consume("<="))
      node = new_binary(ND_LE, node, add());
    else if (consume(">"))
      node = new_binary(ND_LT, add(), node);
    else if (consume(">="))
      node = new_binary(ND_LE, add(), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for(;;) {
    if (consume("+"))
      node = new_binary(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_binary(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for(;;) {
    if (consume("*"))
      node = new_binary(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_binary(ND_DIV, node, unary());
    else
      return node;
  }
}

Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_binary(ND_SUB, new_num(0), primary());
  return primary();
}

// func-args = "(" (assign ("," assign )* )? ")"
Node *func_args() {
  if(consume(")"))
    return NULL;

  Node *head = assign();
  Node *cur = head;
  while(consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

// primary = num
//         | ident func-args?
//         | "(" expr ")"
Node *primary() {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok = consume_ident();
  if (tok) {
    Node *node = calloc(1, sizeof(Node));
    // Function call
    if (consume("(")) {
      node->kind = ND_FUNCCALL;
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args();
      consume(")");
    } else {
      node->kind = ND_LVAR;
      LVar *lvar = find_lvar(tok);
      if (lvar) {
        node->var = lvar;
      } else {
        lvar = calloc(1, sizeof(LVar));
        lvar->next = locals;
        lvar->name = strndup(tok->str, tok->len);
        if(locals)
          lvar->offset = locals->offset + 8;
        node->var = lvar;
        locals = lvar;
      }
    }
    return node;
  }

  // そうでなければ数値のはず
  return new_num(expect_number());
}
