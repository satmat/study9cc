#include "9cc.h"

char *user_input;

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  user_input = argv[1];
  tokenize();
  Program *prog = program();

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    int offset = 0;
    for (LVar *lv = fn->locals; lv; lv = lv->next) {
      offset = align_to(offset, 4);  // int型のoffset
      offset += 4;  // int型のsize
      lv->offset = offset;
    }
    fn->stack_size = align_to(offset, 8);

  }

  codegen(prog);

  return 0;
}
