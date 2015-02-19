#include "vm.h"

#include "builtins.h"
#include "gc.h"
#include "get_file_contents.h"
#include "lang_utils.h"
#include "parser.h"
#include "value.h"
#include "utils/error.h"
#include "value/array.h"
#include "value/builtin_function.h"
#include "value/boolean.h"
#include "value/dictionary.h"
#include "value/integer.h"
#include "value/floating_point.h"
#include "value/function.h"
#include "value/nil.h"
#include "value/string.h"
#include "value/symbol.h"

#include <boost/variant/get.hpp>
#include <boost/filesystem.hpp>

using namespace vv;

vm::machine::machine(std::shared_ptr<call_frame> frame,
                     const std::function<void(vm::machine&)>& exception_handler)
  : frame               {frame},
    retval              {nullptr},
    m_base              {frame},
    m_exception_handler {exception_handler}
{
  gc::set_current_frame(frame);
  gc::set_current_retval(retval);
}

void vm::machine::run()
{
  while (frame->instr_ptr.size()) {
    // Get next instruction (and argument, if it exists), and increment the
    // instruction pointer
    auto command = frame->instr_ptr.front();
    frame->instr_ptr = frame->instr_ptr.subvec(1);
    run_single_command(command);
  }
}

void vm::machine::run_cur_scope()
{
  auto* exit_scope = frame.get();

  while (frame->instr_ptr.size()) {
    // Get next instruction (and argument, if it exists), and increment the
    // instruction pointer
    auto command = frame->instr_ptr.front();
    frame->instr_ptr = frame->instr_ptr.subvec(1);

    if (command.instr == vm::instruction::ret && frame.get() == exit_scope) {
      ret(boost::get<bool>(command.arg));
      return;
    } else if (command.instr == vm::instruction::except) {
      except_until(exit_scope);
    } else {
      run_single_command(command);
    }
  }
}

// Instruction implementations {{{

void vm::machine::push_bool(bool val)
{
  retval = gc::alloc<value::boolean>( val );
}

void vm::machine::push_flt(double val)
{
  retval = gc::alloc<value::floating_point>( val );
}

void vm::machine::push_fn(const function_t& val)
{
  retval = gc::alloc<value::function>( val.argc, val.body, frame );
}

void vm::machine::push_int(int val)
{
  retval = gc::alloc<value::integer>( val );
}

void vm::machine::push_nil()
{
  retval = gc::alloc<value::nil>( );
}

void vm::machine::push_str(const std::string& val)
{
  retval = gc::alloc<value::string>( val );
}

void vm::machine::push_sym(symbol val)
{
  retval = gc::alloc<value::symbol>( val );
}

void vm::machine::push_type(const type_t& type)
{
  read(type.parent);
  auto parent = retval;
  push();
  std::unordered_map<symbol, value::base*> methods;
  for (const auto& i : type.methods) {
    push_fn(i.second);
    auto method = retval;
    push();
    methods[i.first] = method;
  }
  retval = gc::alloc<value::type>( nullptr, methods, *parent, type.name );

  // Clear pushed arguments without touching retval
  frame->pushed.erase(end(frame->pushed) - methods.size() - 1,
                      end(frame->pushed));
  let(type.name);
}

void vm::machine::make_arr(int size)
{
  std::vector<value::base*> args{end(frame->pushed) - size, end(frame->pushed)};
  retval = gc::alloc<value::array>( args );
  frame->pushed.erase(end(frame->pushed) - size, end(frame->pushed));
}

void vm::machine::make_dict(int size)
{
  std::unordered_map<value::base*, value::base*> dict;
  for (auto i = end(frame->pushed) - size; i != end(frame->pushed); i += 2) {
    dict[i[0]] = i[1];
  }
  retval = gc::alloc<value::dictionary>( dict );
  frame->pushed.erase(end(frame->pushed) - size, end(frame->pushed));
}

void vm::machine::read(symbol sym)
{
  auto cur_frame = frame;
  while (cur_frame) {
    auto holder = find_if(rbegin(cur_frame->local), rend(cur_frame->local),
                          [&](const auto& vars) { return vars.count(sym); });
    if (holder != rend(cur_frame->local)) {
      retval = holder->at(sym);
      return;
    }
    cur_frame = cur_frame->enclosing;
  }
  push_str("no such variable: " + to_string(sym));
  except();
}

void vm::machine::write(symbol sym)
{
  auto cur_frame = frame;
  while (cur_frame) {
    auto holder = find_if(rbegin(cur_frame->local), rend(cur_frame->local),
                          [&](const auto& vars) { return vars.count(sym); });
    if (holder != rend(cur_frame->local)) {
      holder->at(sym) = retval;
      return;
    }
    cur_frame = cur_frame->enclosing;
  }
  push_str("no such variable: " + to_string(sym));
  except();
}

void vm::machine::let(symbol sym)
{
  frame->local.back()[sym] = retval;
}

void vm::machine::self()
{
  auto cur_frame = frame;
  while (cur_frame && !cur_frame->self)
    cur_frame = cur_frame->enclosing;
  if (cur_frame) {
    retval = &*cur_frame->self;
  } else {
    push_str("self does not exist outside of objects");
    except();
  }
}

void vm::machine::push_arg()
{
  frame->pushed.push_back(retval);
}

void vm::machine::arg(int idx)
{
  retval = get_arg(*this, idx);
}

void vm::machine::readm(symbol sym)
{
  frame->pushed_self = *retval;
  if (retval->members.count(sym)) {
    retval = retval->members[sym];
    return;
  }

  auto member = find_method(retval->type, sym);
  if (member) {
    retval = member;
    return;
  }
  push_str("no such member: " + to_string(sym));
  except();
}

void vm::machine::writem(symbol sym)
{
  auto value = frame->pushed.back();
  frame->pushed.pop_back();

  retval->members[sym] = value;
  retval = value;
}

// TODO: make suck less.
void vm::machine::call(int argc)
{
  const static std::array<vm::command, 1> com{{ {vm::instruction::ret, false} }};
  if (auto fn = dynamic_cast<value::function*>(retval)) {
    if (argc != fn->argc) {
      push_str("Wrong number of arguments--- expected "
              + std::to_string(fn->argc) + ", got "
              + std::to_string(argc));
      except();
      return;
    };

    frame = std::make_shared<call_frame>(fn->body, frame, fn->enclosure, argc);
    frame->caller = *fn;

    gc::set_current_frame(frame);

  } else if (auto fn = dynamic_cast<value::builtin_function*>(retval)) {
    if (argc != fn->argc) {
      push_str("Wrong number of arguments--- expected "
              + std::to_string(fn->argc) + ", got "
              + std::to_string(argc));
      except();
      return;
    };

    frame = std::make_shared<call_frame>(frame->instr_ptr, frame, m_base, argc);
    frame->caller = *fn;

    try {
      gc::set_current_frame(frame);
      retval = fn->body(*this);
      frame->instr_ptr = com;
    } catch (const vm_error& err) {
      retval = err.error();
      except();
      return;
    }

  } else {
    push_str("Only functions can be called");
    except();
  }
}

void vm::machine::new_obj(int argc)
{
  if (retval->type != &builtin::type::custom_type) {
    push_str("Objects can only be constructed from Types");
    except();
    return;
  }
  auto type = static_cast<value::type*>(retval);

  // Since everything inherits from Object, we're guaranteed to find a
  // constructor
  auto ctr_type = type;
  while (!ctr_type->constructor)
    ctr_type = &static_cast<value::type&>(ctr_type->parent);
  retval = ctr_type->constructor();
  // Hacky--- builtins that can't be instantiated directly have provided
  // constructors that return nullptr, hence this message. The alternative would
  // be them trying to except within their constructors, and tying that behavior
  // back in with the VM can get pretty hairy (as in vm::call)
  if (!retval) {
    push_str("Cannot construct object of type " + type->value());
    except();
    return;
  }
  retval->type = type;

  frame->pushed_self = *retval;
  push_fn(type->init_shim);
  call(argc);
}

void vm::machine::eblk()
{
  frame->local.emplace_back();
}

void vm::machine::lblk()
{
  frame->local.pop_back();
}

void vm::machine::ret(bool copy)
{
  if (frame->parent) {
    frame->parent->pushed.erase(end(frame->parent->pushed) - frame->args,
                                end(frame->parent->pushed));
    if (copy) {
      for (const auto& i : frame->local)
        for (const auto& var : i)
          frame->parent->local.back()[var.first] = var.second;
    }
    frame = frame->parent;
    gc::set_current_frame(frame);
  } else {
    push_str("The top-level environment can't be returned from");
    except();
  }
}

void vm::machine::push()
{
  frame->pushed.push_back(retval);
}

void vm::machine::pop()
{
  retval = frame->pushed.back();
  frame->pushed.pop_back();
}

void vm::machine::req(const std::string& filename)
{
  auto tok_res = get_file_contents(filename);
  if (!tok_res.successful()) {
    push_str(tok_res.error());
    except();
    return;
  }
  auto cur = boost::filesystem::current_path();
  tok_res.result().emplace_back(instruction::chdir, cur.native());
  tok_res.result().emplace_back(instruction::ret, true);
  push_fn({ 0, move(tok_res.result()) });
  call(0);
}

void vm::machine::jmp(int offset)
{
  frame->instr_ptr = frame->instr_ptr.shifted_by(offset - 1);
}

void vm::machine::jmp_false(int offset)
{
  if (!truthy(retval))
    jmp(offset);
}

void vm::machine::jmp_true(int offset)
{
  if (truthy(retval))
    jmp(offset);
}

void vm::machine::push_catch()
{
  frame->catcher = *retval;
}

void vm::machine::pop_catch()
{
  frame->catcher = {};
}

void vm::machine::except()
{
  while (frame->parent && !frame->catcher)
    frame = frame->parent;

  if (!frame->catcher) {
    m_exception_handler(*this);
    // If we're still here, stop executing code since obviously some invariant's
    // broken
    frame->instr_ptr = frame->instr_ptr.subvec(frame->instr_ptr.size());
  } else {
    push_arg();
    retval = &*frame->catcher;
    pop_catch();
    call(1);
  }
}

void vm::machine::chdir(const std::string& dir)
{
  boost::filesystem::current_path(dir);
}

size_t depth(vm::call_frame* frame)
{
  size_t depth{0};
  while (frame->parent) {
    ++depth;
    frame = frame->parent.get();
  }
  return depth;
}

void vm::machine::run_single_command(const vm::command& command)
{
  using boost::get;

  auto instr = command.instr;
  const auto& arg = command.arg;

  // HACK--- avoid weirdness like the following:
  //   let i = 1
  //   let add = i.add // pushed_self is now i
  //   add(2)          // => 3
  //   5 + 1           // pushed self is now 5
  //   add(2)          // => 7
  if (instr != instruction::call)
    frame->pushed_self = {};

  switch (instr) {
  case instruction::push_bool: push_bool(get<bool>(arg));       break;
  case instruction::push_flt:  push_flt(get<double>(arg));      break;
  case instruction::push_fn:   push_fn(get<function_t>(arg));   break;
  case instruction::push_int:  push_int(get<int>(arg));         break;
  case instruction::push_nil:  push_nil();                      break;
  case instruction::push_str:  push_str(get<std::string>(arg)); break;
  case instruction::push_sym:  push_sym(get<symbol>(arg));      break;
  case instruction::push_type: push_type(get<type_t>(arg));     break;

  case instruction::make_arr:  make_arr(get<int>(arg));  break;
  case instruction::make_dict: make_dict(get<int>(arg)); break;

  case instruction::read:  read(get<symbol>(arg));  break;
  case instruction::write: write(get<symbol>(arg)); break;
  case instruction::let:   let(get<symbol>(arg));   break;

  case instruction::self:     self();                   break;
  case instruction::push_arg: push_arg();               break;
  case instruction::arg:      this->arg(get<int>(arg)); break;
  case instruction::readm:    readm(get<symbol>(arg));  break;
  case instruction::writem:   writem(get<symbol>(arg)); break;
  case instruction::call:     call(get<int>(arg));      break;
  case instruction::new_obj:  new_obj(get<int>(arg));   break;

  case instruction::eblk: eblk();              break;
  case instruction::lblk: lblk();              break;
  case instruction::ret:  ret(get<bool>(arg)); break;

  case instruction::push: push(); break;
  case instruction::pop:  pop();  break;

  case instruction::req: req(get<std::string>(arg)); break;

  case instruction::jmp:       jmp(get<int>(arg));       break;
  case instruction::jmp_false: jmp_false(get<int>(arg)); break;
  case instruction::jmp_true:  jmp_true(get<int>(arg));  break;

  case instruction::push_catch: push_catch(); break;
  case instruction::pop_catch:  pop_catch();  break;
  case instruction::except:     except();     break;

  case instruction::chdir:      chdir(get<std::string>(arg)); break;
  }
}

void vm::machine::except_until(vm::call_frame* until)
{
  while (frame.get() != until && !frame->catcher)
    frame = frame->parent;

  if (!frame->catcher) {
    throw vm_error{retval};

  } else {
    push_arg();
    retval = &*frame->catcher;
    pop_catch();
    call(1);
  }
}

// }}}
