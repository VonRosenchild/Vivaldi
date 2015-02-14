#ifndef VV_VALUE_BOOLEAN_H
#define VV_VALUE_BOOLEAN_H

#include "value.h"

namespace vv {

namespace value {

struct boolean : public base {
public:
  boolean(bool val = true);

  std::string value() const override;

  bool val;
};

}

}

#endif
