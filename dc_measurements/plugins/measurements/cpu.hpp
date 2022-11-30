
#ifndef DC_MEASUREMENTS__PLUGINS__MEASUREMENTS__CPU_HPP_
#define DC_MEASUREMENTS__PLUGINS__MEASUREMENTS__CPU_HPP_

#include <chrono>
#include <memory>
#include <string>

#include "dc_core/measurement.hpp"
#include "dc_measurements/measurement.hpp"
#include "dc_measurements/measurement_server.hpp"
#include "system/linux_parser.hpp"
#include "system/system.hpp"

namespace dc_measurements
{

class Cpu : public dc_measurements::Measurement
{
public:
  Cpu();
  ~Cpu();
  dc_interfaces::msg::StringStamped collect() override;

private:
  System system_;
  int max_processes_;
  float cpu_min_;

protected:
  /**
   * @brief Configuration of behavior action
   */
  void onConfigure() override;
  void setValidationSchema() override;
  void setProcessesUsage(json& data_json);
  void setAverageCpu(json& data_json);
  void setValidationSchema(json& data_json);
};

}  // namespace dc_measurements

#endif  // DC_MEASUREMENTS__PLUGINS__MEASUREMENTS__CPU_HPP_