#include "range.h"

#include "builtins.h"
#include "gc.h"

using namespace vv;

value::range::range(object& new_start, object& new_end)
  : object  {&builtin::type::range},
    start {&new_start},
    end   {&new_end}
{ }

value::range::range()
  : object  {&builtin::type::range},
    start {nullptr},
    end   {nullptr}
{ }

std::string value::range::value() const
{
  return start->value() + " to " + end->value();
}

void value::range::mark()
{
  object::mark();
  // We need to ensure neither start not end are nullptr, since this could be
  // happening between allocation and initialization
  if (start && !start->marked())
    gc::mark(*start);
  if (end && !end->marked())
    gc::mark(*end);
}
