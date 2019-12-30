#include "9cc.h"

static char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int labelseq = 0;
char *funcname;

// Pushes the given node's address to the stack.
static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_LVAR:
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  lea rax, [rbp-%d]\n", node->var->offset);
    printf("  push rax\n");
    return;
  case ND_DEREF:
    gen(node->lhs);
    return;
  }
}

static void gen_lval(Node *node) {
  gen_addr(node);
}

static void load(Type *ty) {
  printf("  pop rax\n");

  if (ty->size == 4) {
    printf("  movsxd rax, dword ptr [rax]\n");
  } else {
    assert(ty->size == 8);
    printf("  mov rax, [rax]\n");
  }
  printf("  push rax\n");
}

static void store(Type *ty) {
  printf("  pop rdi\n");
  printf("  pop rax\n");

  if (ty->size == 4) {
    printf("  mov [rax], edi\n");
  } else {
    assert(ty->size == 8);
    printf("  mov [rax], rdi\n");
  }

  printf("  push rdi\n");
}

void gen(Node *node) {
  if (node->kind == ND_NULL) {
    return;
  } else if (node->kind == ND_RETURN) {
    if (node->rhs) {
      gen(node->rhs);
      printf("  pop rax\n");
    }
    printf("  jmp .L.return.%s\n", funcname);
    return;
  } else if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  } else if (node->kind == ND_LVAR) {
    gen_addr(node);
    load(node->ty);
    return;
  } else if (node->kind == ND_FUNCCALL) {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen(arg);
      nargs++;
    }

    // c->b->aの順でstackに積むので
    // 第1引数から順にa->b->cとなるように下ろす
    for (int i = 0; i <= nargs - 1; i++)
      printf("  pop %s\n", argreg8[i]);

    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .L.call.%d\n", labelseq);
    printf("  mov rax, 0\n");
    printf("  call %s\n", node->funcname);
    printf("  jmp .L.end.%d\n", labelseq);
    printf(".L.call.%d:\n", labelseq);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    printf("  call %s\n", node->funcname);
    printf("  add rsp, 8\n");
    printf(".L.end.%d:\n", labelseq);
    printf("  push rax\n");
    labelseq++;
    return;
  } else if (node->kind == ND_ASSIGN) {
    gen_lval(node->lhs);
    gen(node->rhs);
    store(node->ty);
    return;
  } else if (node->kind == ND_WHILE) {
    printf(".Lbegin%d:\n", labelseq);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", labelseq);
    gen(node->then);
    printf("  jmp .Lbegin%d\n", labelseq);
    printf(".Lend%d:\n", labelseq);
    labelseq++;
    return;
  } else if (node->kind == ND_FOR ) {
    if(node->init) {
      gen(node->init);
    }
    printf(".Lbegin%d:\n", labelseq);
    if(node->cond){
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lend%d\n", labelseq);
    }
    if(node->inc){
      gen(node->inc);
    }
    gen(node->then);
    printf("  jmp .Lbegin%d\n", labelseq);
    printf(".Lend%d:\n", labelseq);
    labelseq++;
    return;
  } else if (node->kind == ND_IF ) {
    if(node->els){
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lelse%d\n", labelseq);
      gen(node->then);
      printf("  jmp .Lend%d\n", labelseq);
      printf(".Lelse%d:\n", labelseq);
      gen(node->els);
      printf(".Lend%d:\n", labelseq);
    } else {
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lend%d\n", labelseq);
      gen(node->then);
      printf(".Lend%d:\n", labelseq);
    }
    labelseq++;
    return;
  } else if (node->kind == ND_BLOCK) {
    for(Node* n = node->body; n; n = n->next)
      gen(n);
    return;
  } else if (node->kind == ND_ADDR) {
    gen_addr(node->lhs);
    return;
  } else if (node->kind == ND_DEREF) {
    gen(node->lhs);
    load(node->ty);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_PTR_ADD:
      printf("  imul rdi, %d\n", node->ty->base->size);
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_SUB:
      printf("  imul rdi, %d\n", node->ty->base->size);
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_DIFF:
      printf("  sub rax, rdi\n");
      printf("  cqo\n");
      printf("  mov rdi, %d\n", node->lhs->ty->base->size);
      printf("  idiv rdi\n");
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
  }

  printf("  push rax\n");
}

static void emit_data(Program *prog) {
  for (Var *vl = prog->globals; vl; vl = vl->next)
    printf(".global %s\n", vl->name);

  printf(".bss\n");

  for (Var *vl = prog->globals; vl; vl = vl->next) {
    printf(".align %d\n", vl->ty->align);
    printf("%s:\n", vl->name);
    printf("  .zero %d\n", vl->ty->size);
  }

  printf(".data\n");

  for (Var *vl = prog->globals; vl; vl = vl->next) {
    printf(".align %d\n", vl->ty->align);
    printf("%s:\n", vl->name);
  }
}

void load_arg(Var *var, int idx) {
  // int
  printf("  mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
}

void emit_text(Program *prog) {
  printf(".text\n");

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    printf(".global %s\n", fn->name);
    printf("%s:\n", fn->name);
    funcname = fn->name;

    // Prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // Push arguments to the stack
    int i = 0;
    for (Var *lv = fn->params; lv; lv = lv->next)
      load_arg(lv, i++);

      // Emit code
      for (Node *node = fn->node; node; node =node->next)
        gen(node);

      // Epilogue
      printf(".L.return.%s:\n", funcname);
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
  }
}

void codegen(Program *prog) {
  printf(".intel_syntax noprefix\n");
  emit_data(prog);
  emit_text(prog);
}
