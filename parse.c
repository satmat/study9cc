#include "9cc.h"

Type *char_type = &(Type){ TY_CHAR, 1, 1};
Type *int_type = &(Type){ TY_INT, 4, 4 };

// All local variable instances created during parsing are
// accumulated to this list.
static Var *locals;

// Likewise, global variables are accumulated to this list.
static Var *globals;

static Type *basetype(void);
static Type *declarator(Type *ty, char **name);
static Type *type_suffix(Type*);
static void global_var(void);
static Node *declaration(void);
static bool is_typename(void);
static Node *stmt(void);
static Node *stmt2(void);
static Node *add(void);
static long const_expr(void);
static Node *postfix(void);


Var *find_var(Token *tok) {
  // 本来はスコープ対応すべきだが実装簡略化のため省いている
  for (Var *var = locals; var; var = var->next)
    if(strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
      return var;
  for (Var *var = globals; var; var = var->next)
    if(strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len))
      return var;
  return NULL;
}

static Var *new_var(char *name, Type *ty, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->ty = ty;
  var->is_local = is_local;
  return var;
}

static Var *new_lvar(char *name, Type *ty) {
  Var *var = new_var(name, ty, true);
  var->next = locals;
  locals = var;
  return var;
}

static Var *new_gvar(char *name, Type *ty, bool is_static) {
  Var *var = new_var(name, ty, false);
  var->is_static = is_static;
  var->next = globals;
  globals = var;
  return var;
}

static char *new_label(void) {
  static int cnt = 0;
  char buf[20];
  sprintf(buf, ".L.data.%d", cnt++);
  return strndup(buf, 20);
}

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

bool is_integer(Type *ty) {
  TypeKind k = ty->kind;
  return k == TY_CHAR ||  k == TY_INT;
}

int align_to(int n, int align) {
  return (n + align - 1) & ~(align - 1);
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

Type *array_of(Type *base, int len){
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type *func_type(Type *return_ty) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
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

static Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

static Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  node->ty = int_type;  // 整数のみでintと仮定
  return node;
}

static Node *new_var_node(Var *var) {
  Node *node = new_node(ND_VAR);
  node->var = var;
  return node;
}

// Determine whether the next top-level item is a function
// or a lobal variable by looking ahead input tokens.
static bool is_function(void) {
  Token *tok = token;
  bool isfunc = false;

  Type *ty = basetype();

  if(!consume(";")) {
    char *name = NULL;
    declarator(ty, &name);
    isfunc = name && consume("(");
  }

  token = tok;
  return isfunc;
}

// program = function*
Program *program(void) {
  Function head = {};
  Function *cur = &head;
  globals = NULL;

  while (!at_eof()) {
    if (is_function()) {
      Function *fn = function();
      if(!fn)
        continue;
      cur->next = fn;
      cur = cur->next;
      continue;
    }
    global_var();
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->globals = globals;
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
    node->ty = int_type;
  case ND_ASSIGN:
  case ND_PTR_ADD:
  case ND_PTR_SUB:
  case ND_PTR_DIFF:
    node->ty = node->lhs->ty;
    return;
  case ND_VAR:
    node->ty = node->var->ty;
    return;
  case ND_ADDR:
    if (node->lhs->ty->kind == TY_ARRAY)
      node->ty = pointer_to(node->lhs->ty->base);
    else
      node->ty = pointer_to(node->lhs->ty);
    return;
  case ND_DEREF:
    if (!node->lhs->ty->base)
      error_at(token->str, "ポインタが不正です。");
    node->ty = node->lhs->ty->base;
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
    INT  = 1 << 8,
    CHAR = 1 << 4,
  };

  Type *ty = int_type;
  int counter = 0;

  while (is_typename()) {
    Token *tok = token;

    if (consume("char"))
      counter += CHAR;

    if (consume("int"))
      counter += INT;

    switch (counter) {
      case CHAR:
        ty = char_type;
        break;
      case INT:
        ty = int_type;
        break;
      default:
        error_at(tok->str, "無効な型です。");
    }
  }
  return ty;
}

// declarator = "*"* ident type-suffix
static Type *declarator(Type *ty, char **name) {
  while (consume("*"))
    ty = pointer_to(ty);

  *name = expect_ident();
  return type_suffix(ty);
}

// type-suffix = ("[" const-expr? "]" type-suffix)?
static Type *type_suffix(Type *ty) {
  if (!consume("["))
    return ty;

  int sz = 0;
  if (!consume("]")) {
    sz = const_expr();
    expect("]");
  }

  Token *tok = token;
  ty = type_suffix(ty);

  ty = array_of(ty, sz);
  return ty;
}


// param = declarator
Var *read_func_param(void) {
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);

  Var *lv = new_lvar(name, ty);
  return lv;
}

// params = param ("," param)*
void read_func_params(Function *fn) {
  if (consume(")"))
    return;

  Token *tok = token;

//  fn->params = read_func_param();
//  Var *cur = fn->params;
  Var *cur = read_func_param();

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

static Initializer *new_init_val(Initializer *cur, int sz, int val) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->sz = sz;
  init->val = val;
  cur->next = init;
  return init;
}

static Initializer *gvar_init_string(char *p, int len) {
  Initializer head = {};
  Initializer *cur = &head;
  for (int i = 0; i < len; i++)
    cur = new_init_val(cur, 1, p[i]);
  return head.next;
}

// global-var = basetype declarator type-suffix ";"
static void global_var(void) {
  Type *ty = basetype();
  if (consume(";"))
    return;

  char *name = NULL;
  Token *tok = token;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);

  new_gvar(name, ty, false);
  expect(";");
  return;
}

// declaration = basetype declarator type-suffix ";"
static Node *declaration(void) {
  Token *tok = token;
  Type *ty = basetype();

  char *name = NULL;
  ty = declarator(ty, &name);
  ty = type_suffix(ty);
  new_lvar(name, ty);

  if(consume(";")) {
    return new_node(ND_NULL);
  }

  error_at(tok->str, "型定義の方法が不正です。");
  return NULL;
}

// 次のトークンが型を示すものであればtrueを返す
static bool is_typename(void) {
  return peek("int") || peek("char");
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

// 実装が大変なので数値リテラルのみ対応
static long eval2(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      return node->val;
  }

  error("not a constant expression.");
  return 0;
}

static long eval(Node *node) {
  return eval2(node);
}

static long const_expr(void) {
  // 条件演算子は未対応
  return eval(expr());
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

static Node *new_add(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_ADD, lhs, rhs);
  if (is_integer(lhs->ty) && rhs->ty->base)
    return new_binary(ND_PTR_ADD, rhs, lhs);
  error_at(token->str, "不正なオペランドです。");
}

static Node *new_sub(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_SUB, lhs, rhs);
  if (lhs->ty->base && rhs->ty->base)
    return new_binary(ND_PTR_DIFF, lhs, rhs);
  error_at(token->str, "不正なオペランドです。");
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
  Node *node = mul();

  for(;;) {
    if (consume("+"))
      node = new_add(node, mul());
    else if (consume("-"))
      node = new_sub(node, mul());
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
//       | postfix
Node *unary() {
  if (consume("+"))
    return primary();
  if (consume("-"))
    return new_binary(ND_SUB, new_num(0), primary());
  if (consume("*"))
    return new_unary(ND_DEREF, unary());
  if (consume("&"))
    return new_unary(ND_ADDR, unary());
  return postfix();
}

// postfix = primary ("[" expr "]")*
static Node *postfix(void) {
  Node *node = primary();
  for(;;) {
    if (consume("[")) {
      // x[y] is short for *(x+y)
      Node  *exp = new_add(node, expr());
      expect("]");
      node = new_unary(ND_DEREF, exp);
      continue;
    }
    return node;
  }
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
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | str
//         | num
Node *primary() {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok;

  if( consume("sizeof") ) {
    tok = token;
    if (consume("(")) {
      if (is_typename()) {
        Type *ty = basetype();
        expect(")");
        return new_num(ty->size);
      }
      token = tok->next;
    }

    Node *node = unary();
    expect(")");
    add_type(node);
    return new_num(node->ty->size);
  }

  if (tok = consume_ident()) {
    // Function call
    if (consume("(")) {
      Node *node = new_node(ND_FUNCCALL);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args();
      node->ty = int_type;  // グローバル変数を導入するまで暫定
      consume(")");
      add_type(node);
      return node;
    }

    // Variable
    Var *var = find_var(tok);
    if (var) {
      return new_var_node(var);
    }
    error_at(tok->str, "未定義の変数です。");
  }

  tok = token;
  if (token->kind == TK_STR) {
    token = token->next;  // returnする前に次にすすめる

    Type *ty = array_of(char_type, tok->cont_len);
    Var *var = new_gvar(new_label(), ty, true);
    var->initializer = gvar_init_string(tok->contents, tok->cont_len);
    return new_var_node(var);
  }

  // そうでなければ数値のはず
  return new_num(expect_number());
}
