#include "except.h"

#include "vm/instruction.h"

using namespace vv;

ast::except::except(std::unique_ptr<expression>&& value)
  : m_value {move(value)}
{ }

std::vector<vm::command> ast::except::generate() const
{
  auto vec = m_value->code();
  vec.emplace_back(vm::instruction::exc);
  return vec;
}
