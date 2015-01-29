#ifndef IL_VALUE_SYMBOL_H
#define IL_VALUE_SYMBOL_H

#include "../symbol.h"
#include "value.h"

namespace il {

namespace value {

class symbol : public base {
public:
  symbol(const il::symbol& val) : m_val{val} { }

  custom_type* type() const override;
  std::string value() const override;

  base* copy() const override;

private:
  il::symbol m_val;
};

}

}

#endif
