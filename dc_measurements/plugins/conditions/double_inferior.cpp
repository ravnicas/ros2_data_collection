#include "dc_measurements/plugins/conditions/double_inferior.hpp"

namespace dc_conditions
{

DoubleInferior::DoubleInferior() : dc_conditions::Condition()
{
}

void DoubleInferior::onConfigure()
{
  auto node = getNode();
  nav2_util::declare_parameter_if_not_declared(node, condition_name_ + ".key", rclcpp::PARAMETER_STRING);
  nav2_util::declare_parameter_if_not_declared(node, condition_name_ + ".value", rclcpp::PARAMETER_DOUBLE);
  nav2_util::declare_parameter_if_not_declared(node, condition_name_ + ".include_value", rclcpp::ParameterValue(true));
  node->get_parameter(condition_name_ + ".key", key_);
  node->get_parameter(condition_name_ + ".value", value_);
  node->get_parameter(condition_name_ + ".include_value", include_value_);
}

bool DoubleInferior::getState(dc_interfaces::msg::StringStamped msg)
{
  json data_json = json::parse(msg.data);
  json flat_json = data_json.flatten();

  std::string key_w_prefix = std::string("/") + key_;
  if (include_value_)
  {
    return (flat_json.contains(key_w_prefix) && flat_json[key_w_prefix].type() == json::value_t::number_float &&
            flat_json[key_w_prefix] <= value_);
  }
  else
  {
    return (flat_json.contains(key_w_prefix) && flat_json[key_w_prefix].type() == json::value_t::number_float &&
            flat_json[key_w_prefix] < value_);
  }
}

DoubleInferior::~DoubleInferior() = default;

}  // namespace dc_conditions

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(dc_conditions::DoubleInferior, dc_core::Condition)
