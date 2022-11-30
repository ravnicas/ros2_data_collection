#ifndef DC_MEASUREMENTS__MEASUREMENT_HPP_
#define DC_MEASUREMENTS__MEASUREMENT_HPP_

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <utility>

#include "dc_core/condition.hpp"
#include "dc_core/measurement.hpp"
#include "dc_interfaces/msg/string_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/create_timer_ros.h"
#include "tf2_ros/transform_listener.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "tf2/utils.h"
#pragma GCC diagnostic pop

namespace dc_measurements
{

enum class Status : int8_t
{
  SUCCEEDED = 1,
  FAILED = 2,
  RUNNING = 3,
};

using namespace std::chrono_literals;  // NOLINT
using json = nlohmann::json;
using nlohmann::json_schema::json_validator;

/**
 * @class nav2_behaviors::Behavior
 * @brief An action server Behavior base class implementing the action server and basic factory.
 */
// template <typename ActionT>
class Measurement : public dc_core::Measurement
{
public:
  //   using ActionServer = nav2_util::SimpleActionServer<ActionT>;

  /**
   * @brief A Measurement constructor
   */
  Measurement()
  {
  }

  virtual ~Measurement() = default;

  // an opportunity for derived classes to set the validation schema
  // if they chose
  virtual void setValidationSchema() = 0;

  void setValidationSchemaFromPath(const std::string& json_schema_path)
  {
    if (enable_validator_)
    {
      validateSchema(json_schema_path);
    }
  }

  // an opportunity for derived classes to do something on configuration
  // if they chose
  virtual void onConfigure()
  {
  }

  // an opportunity for derived classes to do something on cleanup
  // if they chose
  virtual void onCleanup()
  {
  }

  std::shared_ptr<rclcpp_lifecycle::LifecycleNode> getNode()
  {
    auto node = node_.lock();
    if (!node)
    {
      throw std::runtime_error{ "Failed to lock node" };
    }
    return node;
  }

  void validateSchema(const std::string& package_name, const std::string& json_filename)
  {
    std::string package_share_directory = ament_index_cpp::get_package_share_directory(package_name.c_str());
    std::string path = package_share_directory + "/plugins/measurements/" + json_filename.c_str();
    RCLCPP_INFO_STREAM(logger_, "Looking for schema at " << schema_);
    std::ifstream f(path.c_str());
    schema_ = json::parse(f);

    RCLCPP_INFO_STREAM(logger_, "schema: " << schema_);
    try
    {
      validator_.set_root_schema(schema_);
    }
    catch (const std::exception& e)
    {
      std::string err = std::string("Validation of schema failed: ") + e.what();
      RCLCPP_ERROR(logger_, "%s", err.c_str());
      throw std::runtime_error{ err.c_str() };
    }
  }

  void validateSchema(const std::string& json_schema_path)
  {
    std::ifstream f(json_schema_path.c_str());
    schema_ = json::parse(f);

    RCLCPP_INFO_STREAM(logger_, "schema: " << schema_);
    try
    {
      validator_.set_root_schema(schema_);
    }
    catch (const std::exception& e)
    {
      std::string err = std::string("Validation of schema failed: ") + e.what();
      RCLCPP_ERROR(logger_, "%s", err.c_str());
      throw std::runtime_error{ err.c_str() };
    }
  }

  bool isConditionOn()
  {
    bool all_conditions_res = true;
    bool any_conditions_res = true;
    bool none_conditions_res = true;

    // "All" conditions ON enable
    if (!if_all_conditions_.empty())
    {
      all_conditions_res =
          std::all_of(if_all_conditions_.begin(), if_all_conditions_.end(),
                      [&](const std::string& condition) { return conditions_[condition]->getState() == true; });
    }

    // "Any" condition ON enable
    if (!if_any_conditions_.empty())
    {
      // No "any" condition defined, activates
      any_conditions_res =
          std::any_of(if_any_conditions_.begin(), if_any_conditions_.end(),
                      [&](const std::string& condition) { return conditions_[condition]->getState() == true; });
    }

    // All condition set to false condition ON enable
    if (!if_none_conditions_.empty())
    {
      none_conditions_res =
          std::all_of(if_none_conditions_.begin(), if_none_conditions_.end(),
                      [&](const std::string& condition) { return conditions_[condition]->getState() == false; });
    }

    RCLCPP_DEBUG(logger_, "all_conditions_res=%d, any_conditions_res=%d, none_conditions_res=%d", all_conditions_res,
                 any_conditions_res, none_conditions_res);

    return all_conditions_res && any_conditions_res && none_conditions_res;
  }

  bool isAnyConditionSet()
  {
    return (!if_all_conditions_.empty() && !if_any_conditions_.empty() && !if_none_conditions_.empty());
  }

  void collectAndPublish()
  {
    if (enabled_)
    {
      auto msg = collect();
      RCLCPP_DEBUG(logger_, "Measurement: %s, msg: %s", measurement_name_.c_str(),
                   dc_interfaces::msg::to_yaml(msg).c_str());

      // Init publish
      if (msg.data != "" && msg.data != "null" &&
          (init_max_measurements_ == 0 || init_counter_published_ < init_max_measurements_))
      {
        // Add tags
        if (tags_.size() != 0)
        {
          json data_json = json::parse(msg.data);
          data_json["tags"] = tags_;
          msg.data = data_json.dump(-1, ' ', true);
        }
        // Add measurement name
        if (include_measurement_name_)
        {
          json data_json = json::parse(msg.data);
          data_json["name"] = measurement_name_;
          msg.data = data_json.dump(-1, ' ', true);
        }

        // Publish when validation activated
        if (enable_validator_)
        {
          try
          {
            validator_.validate(json::parse(msg.data));
            data_pub_->publish(msg);
            init_counter_published_++;
          }
          catch (const std::exception& e)
          {
            RCLCPP_ERROR_STREAM(logger_, "Validation failed: " << e.what());
          }
        }
        // Publish without validation
        else
        {
          data_pub_->publish(msg);
          init_counter_published_++;
        }
      }
    }
  }

  virtual dc_interfaces::msg::StringStamped collect() = 0;

  // configure the server on lifecycle setup
  void configure(const rclcpp_lifecycle::LifecycleNode::WeakPtr& parent, const std::string& name,
                 std::shared_ptr<tf2_ros::Buffer> tf, const std::string& measurement_plugin,
                 const std::string& group_key, const std::string& topic_output, const int& polling_interval,
                 const bool& debug, const bool& enable_validator, const std::string& json_schema_path,
                 const std::vector<std::string>& tags, const bool& init_collect, const int& init_max_measurements,
                 const bool& include_measurement_name, const std::vector<std::string>& if_all_conditions,
                 const std::vector<std::string>& if_any_conditions, const std::vector<std::string>& if_none_conditions,
                 const rclcpp::CallbackGroup::SharedPtr& timer_cb_group) override
  {
    node_ = parent;
    auto node = node_.lock();

    logger_ = node->get_logger();

    RCLCPP_INFO(logger_, "Configuring %s", name.c_str());

    measurement_plugin_ = measurement_plugin;
    tf_ = tf;
    measurement_name_ = name;
    topic_output_ = topic_output;
    polling_interval_ = polling_interval;
    debug_ = debug;
    enable_validator_ = enable_validator;
    json_schema_path_ = json_schema_path;
    group_key_ = group_key;
    tags_ = tags;
    init_collect_ = init_collect;
    init_max_measurements_ = init_max_measurements;
    include_measurement_name_ = include_measurement_name;
    if_all_conditions_ = if_all_conditions;
    if_any_conditions_ = if_any_conditions;
    if_none_conditions_ = if_none_conditions;
    timer_cb_group_ = timer_cb_group;

    data_pub_ = node->create_publisher<dc_interfaces::msg::StringStamped>(
        std::string("/dc/measurement/") + measurement_name_, 1);
    collect_timer_ =
        node->create_wall_timer(std::chrono::milliseconds(polling_interval_),
                                std::bind(&dc_measurements::Measurement::collectAndPublish, this), timer_cb_group_);

    RCLCPP_INFO(logger_, "Done configuring %s", measurement_name_.c_str());

    if (json_schema_path_.empty())
    {
      setValidationSchema();
    }
    else
    {
      setValidationSchemaFromPath(json_schema_path_);
    }
    onConfigure();

    if (enable_validator_ && schema_.empty())
    {
      throw std::runtime_error{ "Enabled validation but didn't configure schema!" };
    }
  }

  // Cleanup server on lifecycle transition
  void cleanup() override
  {
    data_pub_.reset();
    onCleanup();
  }

  // Activate server on lifecycle transition
  void activate() override
  {
    RCLCPP_INFO(logger_, "Activating %s", measurement_name_.c_str());

    data_pub_->on_activate();
    enabled_ = true;

    if (init_collect_)
    {
      collectAndPublish();
    }
  }

  // Deactivate server on lifecycle transition
  void deactivate() override
  {
    data_pub_->on_deactivate();
    enabled_ = false;
  }

protected:
  rclcpp_lifecycle::LifecycleNode::WeakPtr node_;

  std::string measurement_name_;
  std::string measurement_plugin_;
  double enabled_;
  bool debug_;
  std::shared_ptr<tf2_ros::Buffer> tf_;

  // Publish data
  int polling_interval_;
  rclcpp_lifecycle::LifecyclePublisher<dc_interfaces::msg::StringStamped>::SharedPtr data_pub_;
  rclcpp::TimerBase::SharedPtr collect_timer_;
  std::string topic_output_;
  std::string group_key_;

  // Parameters
  bool init_collect_;
  bool include_measurement_name_;

  // Counters
  int init_counter_published_ = 0;
  int init_max_measurements_;

  // Conditions
  std::map<std::string, std::shared_ptr<dc_core::Condition>> conditions_;
  std::vector<std::string> if_all_conditions_;
  std::vector<std::string> if_any_conditions_;
  std::vector<std::string> if_none_conditions_;

  // Logger
  rclcpp::Logger logger_{ rclcpp::get_logger("dc_measurements") };

  // CB
  rclcpp::CallbackGroup::SharedPtr timer_cb_group_;

  // Validation
  bool enable_validator_;
  std::string json_schema_path_;
  json_validator validator_;
  json schema_;

  // Tags
  std::vector<std::string> tags_;
};

}  // namespace dc_measurements

#endif  // DC_MEASUREMENTS__MEASUREMENT_HPP_