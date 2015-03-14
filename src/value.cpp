#include "value.h"

#include "builtins.h"
#include "gc.h"
#include "ast/function_definition.h"
#include "utils/lang.h"
#include "value/builtin_function.h"
#include "value/function.h"

using namespace vv;

value::base::base(struct type* new_type)
  : members  {},
    type     {new_type},
    m_marked {false}
{ }

value::base::base()
  : members  {},
    type     {&builtin::type::object},
    m_marked {false}
{ }

size_t value::base::hash() const
{
  const static std::hash<const void*> hasher{};
  return hasher(static_cast<const void*>(this));
}

bool value::base::equals(const value::base& other) const
{
  return this == &other;
}

void value::base::mark()
{
  m_marked = true;
  if (type && !type->marked())
    gc::mark(*type);
  for (auto& i : members)
    if (!i.second->marked())
      gc::mark(*i.second);
}

value::basic_function::basic_function(func_type type,
                                      int argc,
                                      vm::environment* enclosing,
                                      vector_ref<vm::command> body)
  : base      {&builtin::type::function},
    type      {type},
    argc      {argc},
    enclosing {enclosing},
    body      {body}
{ }

value::type::type(
    const std::function<value::base*()>& new_constructor,
    const hash_map<vv::symbol, value::base*>& new_methods,
    value::type& new_parent,
    vv::symbol new_name)
  : base        {&builtin::type::custom_type},
    methods     {new_methods},
    constructor {new_constructor},
    parent      {new_parent},
    name        {new_name}
{
  if (auto init = find_method(this, {"init"})) {
    // TODO: make function and builtin_function both inherit from a
    // basic_function class, so I can quit it with all these dynamic_casts.
    if (auto fn = dynamic_cast<builtin_function*>(init))
      init_shim.argc = fn->argc;
    else
      init_shim.argc = static_cast<function*>(init)->argc;

    for (auto i = 0; i != init_shim.argc; ++i) {
      init_shim.body.emplace_back(vm::instruction::arg, i);
    }

    init_shim.body.emplace_back( vm::instruction::self );
    init_shim.body.emplace_back( vm::instruction::readm, vv::symbol{"init"} );
    init_shim.body.emplace_back( vm::instruction::call, init_shim.argc );
    init_shim.body.emplace_back( vm::instruction::self );
    init_shim.body.emplace_back( vm::instruction::ret, false );

  } else {
    init_shim.argc = 0;
    init_shim.body = {
      { vm::instruction::self },
      { vm::instruction::ret, false }
    };
  }
}

std::string value::type::value() const { return to_string(name); }

void value::type::mark()
{
  base::mark();
  for (const auto& i : methods)
    if (!i.second->marked())
      gc::mark(*i.second);
  if (!parent.marked())
    gc::mark(parent);
}

size_t std::hash<vv::value::base*>::operator()(const vv::value::base* b) const
{
  return b->hash();
}

bool std::equal_to<vv::value::base*>::operator()(const vv::value::base* left,
                                                 const vv::value::base* right) const
{
  return left->equals(*right);
}
