#include <string.h>
#include "9cc.h"

Type *int_type = &(Type){ TY_INT, 4, 4};

static LVar *locals;

static Node *declaration(void);
static bool is_typename(void);
static Node *stmt(void);
static Node *stmt2(void);

LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next)
    if(strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
      return var;
  return NULL;
}

LVar *new_var(char *name) {
  LVar *var = calloc(1, sizeof(LVar));
  var->name = name;
  return var;
}

LVar *new_lvar(char *name, Type *ty) {
  LVar *var = new_var(name);
  var->ty = ty;
  var->next = locals;
  locals = var;
  return var;
}

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

int align_to(int n, int align) {
  return (n + align - 1) & ~(align - 1);
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

static Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

static Node *new_num(int val) {
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

void add_type(Node* node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->init);
  add_type(node->cond);
  add_type(node->inc);
  add_type(node->then);
  add_type(node->els);

  for (Node *n = node->body; n; n = n->next)
    add_type(n);
  for (Node *n = node->args; n; n = n->next)
    add_type(n);

  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    //node->ty = long_type;
    node->ty = int_type;
  case ND_ASSIGN:
    node->ty = node->rhs->ty;
    return;
  case ND_LVAR:
    node->ty = node->var->ty;
    return;
  }
}

// basetype = buildin-type
//
// builtin-type = "int"
static Type *basetype() {
  if (!is_typename())
    error_at(token->str, "型名ではありません。");

  enum {
    INT = 1 << 8,
  };

  Type *ty = int_type;
  int counter = 0;

  while (is_typename()) {
    Token *tok = token;

    if (consume("int"))
      counter += INT;

    switch (counter) {
      case INT:
        ty = int_type;
        break;
      default:
        error_at(tok->str, "無効な型です。");
    }
  }
  return ty;
}

// declarator = ident
void declarator(Type *ty, char **name) {
  while (consume("*"))
    ty = pointer_to(ty);
  *name = expect_ident(&name);
}

// param = declarator
LVar *read_func_param(void) {
  Type *ty = basetype();
  char *name = NULL;
  declarator(ty, &name);
  LVar *lv = new_lvar(name, ty);
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

// function = basetype declarator "(" params? ")" ("{" stmt* "}" | ";")
Function *function(void) {
  locals = NULL;

  Type *ty = basetype();
  char *name = NULL;
  declarator(ty, &name);

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

// declaration = 
static Node *declaration(void) {
  Token *tok = token;
  Type *ty = basetype();

  char *name = NULL;
  declarator(ty, &name);
  new_lvar(name, ty);

  if(consume(";")) {
    return new_node(ND_NULL);
  }

  error_at(tok->str, "型定義の方法が不正です。");
  return NULL;
}

// 次のトークンが型を示すものであればtrueを返す
static bool is_typename(void) {
  return peek("int");
}

static Node *stmt(void) {
  Node *node = stmt2();
  add_type(node);
  return node;
}

// stmt2 = expr ";"
//        | "return" expr ";"
//        | "while" "(" expr ")" stmt
//        | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "{" stmt* "}"
//        | ";"
//        | declaration
Node *stmt2() {
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
  } else if (is_typename()) {
    return declaration();
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

// unary = "+"? primary
//       | "-"? primary
//       | "*" unary
//       | "&" unary
Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_binary(ND_SUB, new_num(0), primary());
  if (consume("*"))
    return new_unary(ND_DEREF, unary(), token);
  if (consume("&"))
    return new_unary(ND_ADDR, unary(), token);
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
      return node;
    }

    // Variable
    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->kind = ND_LVAR;
      node->var = lvar;
      return node;
    }
    error_at(tok->str, "未定義の変数です。");
  }

  // そうでなければ数値のはず
  return new_num(expect_number());
}
