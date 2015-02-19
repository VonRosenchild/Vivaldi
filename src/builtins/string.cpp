#include "builtins.h"

#include "gc.h"
#include "lang_utils.h"
#include "utils/string_helpers.h"
#include "value/builtin_function.h"
#include "value/string.h"
#include "value/string_iterator.h"
#include "value/symbol.h"

using namespace vv;
using namespace builtin;

namespace {

int to_int(value::base* boxed)
{
  return static_cast<value::integer*>(boxed)->val;
}

const std::string& to_string(const value::base* boxed)
{
  return static_cast<const value::string*>(boxed)->val;
}

vv::symbol to_symbol(const value::base* boxed)
{
  return static_cast<const value::symbol*>(boxed)->val;
}

// string {{{

value::base* fn_string_init(vm::machine& vm)
{
  auto& str = static_cast<value::string&>(*vm.frame->self);
  auto arg = get_arg(vm, 0);
  if (arg->type == &type::string)
    str.val = to_string(arg);
  else if (arg->type == &type::symbol)
    str.val = to_string(to_symbol(arg));
  else
     str.val = arg->value();
  return &str;
}

value::base* fn_string_size(vm::machine& vm)
{
  auto sz = static_cast<value::string&>(*vm.frame->self).val.size();
  return gc::alloc<value::integer>( static_cast<int>(sz) );
}

value::base* fn_string_equals(vm::machine& vm)
{
  auto arg = get_arg(vm, 0);
  if (arg->type != &type::string)
    return gc::alloc<value::boolean>( false );
  return gc::alloc<value::boolean>(
      to_string(&*vm.frame->self) == to_string(arg) );
}

value::base* fn_string_unequal(vm::machine& vm)
{
  auto arg = get_arg(vm, 0);
  if (arg->type != &type::string)
    return gc::alloc<value::boolean>( false );
  return gc::alloc<value::boolean>(
      to_string(&*vm.frame->self) != to_string(arg) );
}

value::base* fn_string_add(vm::machine& vm)
{
  if (get_arg(vm, 0)->type != &type::string)
    return throw_exception("Only strings can be appended to other strings");

  auto str = to_string(get_arg(vm, 0));
  auto new_str = static_cast<value::string&>(*vm.frame->self).val + str;
  return gc::alloc<value::string>( new_str );
}

value::base* fn_string_times(vm::machine& vm)
{
  if (get_arg(vm, 0)->type != &type::integer)
    return throw_exception("Strings can only be multiplied by Integers");

  auto val = static_cast<value::string&>(*vm.frame->self).val;
  std::string new_str{};
  for (auto i = to_int(get_arg(vm, 0)); i--;)
    new_str += val;
  return gc::alloc<value::string>( new_str );
}

value::base* fn_string_to_int(vm::machine& vm)
{
  return gc::alloc<value::integer>( vv::to_int(to_string(&*vm.frame->self)) );
}

value::base* fn_string_at(vm::machine& vm)
{
  auto arg = get_arg(vm, 0);
  if (arg->type != &type::integer)
    return throw_exception("Index must be an Integer");
  auto val = static_cast<value::integer*>(arg)->val;
  const auto& str = static_cast<value::string&>(*vm.frame->self).val;
  if (str.size() <= static_cast<unsigned>(val) || val < 0)
    return throw_exception("Out of range (expected 0-"
                           + std::to_string(str.size()) + ", got "
                           + std::to_string(val) + ")");
  return gc::alloc<value::string>( std::string{str[static_cast<unsigned>(val)]} );
}

value::base* fn_string_start(vm::machine& vm)
{
  auto& self = static_cast<value::string&>(*vm.frame->self);
  return gc::alloc<value::string_iterator>(self);
}

value::base* fn_string_stop(vm::machine& vm)
{
  auto& self = static_cast<value::string&>(*vm.frame->self);
  auto end = gc::alloc<value::string_iterator>(self);
  static_cast<value::string_iterator*>(end)->idx = self.val.size();
  return end;
}

value::base* fn_string_to_upper(vm::machine& vm)
{
  auto str = static_cast<value::string&>(*vm.frame->self).val;
  transform(begin(str), end(str), begin(str), toupper);
  return gc::alloc<value::string>( str );
}

value::base* fn_string_to_lower(vm::machine& vm)
{
  auto str = static_cast<value::string&>(*vm.frame->self).val;
  transform(begin(str), end(str), begin(str), tolower);
  return gc::alloc<value::string>( str );
}

value::base* fn_string_starts_with(vm::machine& vm)
{
  if (get_arg(vm, 0)->type != &type::string)
    return throw_exception("Strings can only start with other Strings");

  const auto& str = static_cast<value::string&>(*vm.frame->self).val;
  const auto& other = to_string(get_arg(vm, 0));

  if (other.size() > str.size() || !equal(begin(other), end(other), begin(str)))
    return gc::alloc<value::boolean>( false );
  return gc::alloc<value::boolean>( true );
}

value::base* fn_string_ord(vm::machine& vm)
{
  auto str = static_cast<value::string&>(*vm.frame->self).val;
  if (!str.size())
    return throw_exception("Cannot call ord on an empty string");
  return gc::alloc<value::integer>( str[0] );
}

// }}}
// string_iterator {{{

value::base* fn_string_iterator_at_start(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  return gc::alloc<value::boolean>( iter->idx == 0 );
}

value::base* fn_string_iterator_at_end(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  return gc::alloc<value::boolean>( iter->idx == iter->str.val.size() );
}

value::base* fn_string_iterator_get(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  if (iter->idx == iter->str.val.size())
    return throw_exception("StringIterator is at end of string");
  return gc::alloc<value::string>( std::string{iter->str.val[iter->idx]} );
}

value::base* fn_string_iterator_increment(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  if (iter->idx == iter->str.val.size())
    return throw_exception("StringIterators cannot be incremented past end");
  iter->idx += 1;
  return iter;
}

value::base* fn_string_iterator_decrement(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  if (iter->idx == 0)
    return throw_exception("StringIterators cannot be decremented past start");
  iter->idx -= 1;
  return iter;
}

value::base* fn_string_iterator_add(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  if (get_arg(vm, 0)->type != &type::integer)
    return throw_exception("Only numeric types can be added to StringIterators");
  auto offset = to_int(get_arg(vm, 0));

  if (static_cast<int>(iter->idx) + offset < 0)
    return throw_exception("StringIterators cannot be decremented past start");
  if (iter->idx + offset > iter->str.val.size())
    return throw_exception("StringIterators cannot be incremented past end");

  auto other = gc::alloc<value::string_iterator>( *iter );
  static_cast<value::string_iterator*>(other)->idx = iter->idx + offset;
  return other;
}

value::base* fn_string_iterator_subtract(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  if (get_arg(vm, 0)->type != &type::integer)
    return throw_exception("Only numeric types can be added to StringIterators");
  auto offset = to_int(get_arg(vm, 0));

  if (static_cast<int>(iter->idx) - offset < 0)
    return throw_exception("StringIterators cannot be decremented past start");
  if (static_cast<int>(iter->idx) - offset > static_cast<int>(iter->str.val.size()))
    return throw_exception("StringIterators cannot be incremented past end");

  auto other = gc::alloc<value::string_iterator>( *iter );
  static_cast<value::string_iterator*>(other)->idx = iter->idx - offset;
  return other;
}

value::base* fn_string_iterator_equals(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  auto other = static_cast<value::string_iterator*>(get_arg(vm, 0));
  return gc::alloc<value::boolean>( &iter->str == &other->str
                                  && iter->idx == other->idx );
}

value::base* fn_string_iterator_unequal(vm::machine& vm)
{
  auto iter = static_cast<value::string_iterator*>(&*vm.frame->self);
  auto other = static_cast<value::string_iterator*>(get_arg(vm, 0));
  return gc::alloc<value::boolean>( &iter->str != &other->str
                                  || iter->idx != other->idx );
}

// }}}

value::builtin_function string_init        {fn_string_init,        1};
value::builtin_function string_size        {fn_string_size,        0};
value::builtin_function string_equals      {fn_string_equals,      1};
value::builtin_function string_unequal     {fn_string_unequal,     1};
value::builtin_function string_add         {fn_string_add,         1};
value::builtin_function string_times       {fn_string_times,       1};
value::builtin_function string_to_int      {fn_string_to_int,      0};
value::builtin_function string_at          {fn_string_at,          1};
value::builtin_function string_start       {fn_string_start,       0};
value::builtin_function string_stop        {fn_string_stop,        0};
value::builtin_function string_to_upper    {fn_string_to_upper,    0};
value::builtin_function string_to_lower    {fn_string_to_lower,    0};
value::builtin_function string_starts_with {fn_string_starts_with, 1};
value::builtin_function string_ord         {fn_string_ord,         0};

value::builtin_function string_iterator_at_start  {fn_string_iterator_at_start,  0};
value::builtin_function string_iterator_at_end    {fn_string_iterator_at_end,    0};
value::builtin_function string_iterator_get       {fn_string_iterator_get,       0};
value::builtin_function string_iterator_equals    {fn_string_iterator_equals,    1};
value::builtin_function string_iterator_unequal   {fn_string_iterator_unequal,   1};
value::builtin_function string_iterator_increment {fn_string_iterator_increment, 0};
value::builtin_function string_iterator_decrement {fn_string_iterator_decrement, 0};
value::builtin_function string_iterator_add       {fn_string_iterator_add,       1};
value::builtin_function string_iterator_subtract  {fn_string_iterator_subtract,  1};

}

value::type type::string {gc::alloc<value::string>, {
  { {"init"},        &string_init        },
  { {"size"},        &string_size        },
  { {"equals"},      &string_equals      },
  { {"unequal"},     &string_unequal     },
  { {"add"},         &string_add         },
  { {"times"},       &string_times       },
  { {"to_int"},      &string_to_int      },
  { {"at"},          &string_at          },
  { {"start"},       &string_start       },
  { {"stop"},        &string_stop        },
  { {"to_upper"},    &string_to_upper    },
  { {"to_lower"},    &string_to_lower    },
  { {"starts_with"}, &string_starts_with },
  { {"ord"},         &string_ord }
}, builtin::type::object, {"String"}};

value::type type::string_iterator {[]{ return nullptr; }, {
  { {"at_start"},  &string_iterator_at_start  },
  { {"at_end"},    &string_iterator_at_end    },
  { {"get"},       &string_iterator_get       },
  { {"equals"},    &string_iterator_equals    },
  { {"unequal"},   &string_iterator_unequal   },
  { {"increment"}, &string_iterator_increment },
  { {"decrement"}, &string_iterator_decrement },
  { {"add"},       &string_iterator_add       },
  { {"subtract"},  &string_iterator_subtract  },
}, builtin::type::object, {"StringIterator"}};
