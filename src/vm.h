#ifndef IL_VM_H
#define IL_VM_H

#include "vm/call_stack.h"

namespace il {

namespace vm {

class machine {
public:
  machine(std::shared_ptr<call_stack> base);

  void run();

private:
  void push_bool(bool val);
  void push_flt(double val);
  void push_fn(vector_ref<command> val);
  void push_int(int val);
  void push_nil();
  void push_str(const std::string& val);
  void push_sym(symbol val);

  void read(symbol sym);
  void write(symbol sym);
  void let(symbol sym);

  void push_self();
  void self();
  void push_arg();
  void pop_arg(symbol sym);
  void mem(symbol sym);
  void call(int args);

  void eblk();
  void lblk();
  void ret();

  void jmp_false(int offset);
  void jmp(int offset);

  std::shared_ptr<call_stack> m_stack;
  value::base* m_retval;

  std::shared_ptr<call_stack> m_base;
};

}

}

#endif
