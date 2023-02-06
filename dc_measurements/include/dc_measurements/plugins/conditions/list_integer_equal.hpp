#ifndef DC_MEASUREMENTS__PLUGINS__CONDITION__LIST_INTEGER_EQUAL_HPP_
#define DC_MEASUREMENTS__PLUGINS__CONDITION__LIST_INTEGER_EQUAL_HPP_

#include <algorithm>

#include "dc_core/condition.hpp"
#include "dc_interfaces/msg/string_stamped.hpp"
#include "dc_measurements/condition.hpp"
#include "dc_util/json_utils.hpp"
#include "dc_util/string_utils.hpp"
#include "nav2_util/node_utils.hpp"

namespace dc_conditions
{

class ListIntegerEqual : public dc_conditions::Condition
{
public:
  ListIntegerEqual();
  ~ListIntegerEqual();

protected:
  std::string key_;
  std::vector<long int> value_;
  bool order_matters_;
  bool getState(dc_interfaces::msg::StringStamped msg);
  void onConfigure();
};

}  // namespace dc_conditions

#endif  // DC_MEASUREMENTS__PLUGINS__CONDITION__LIST_INTEGER_EQUAL_HPP_
