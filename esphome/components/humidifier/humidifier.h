#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "humidifier_mode.h"
#include "humidifier_traits.h"

namespace esphome {
namespace humidifier {

#define LOG_HUMIDIFIER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
  }

class Humidifier;

/** This class is used to encode all control actions on a Humidifier device.
 *
 * It is supposed to be used by all code that wishes to control a humidifier device (mqtt, api, lambda etc).
 * Create an instance of this class by calling `id(humidifier_device).make_call();`. Then set all attributes
 * with the `set_x` methods. Finally, to apply the changes call `.perform();`.
 *
 * The integration that implements the humidifier device receives this instance with the `control` method.
 * It should check all the properties it implements and apply them as needed. It should do so by
 * getting all properties it controls with the getter methods in this class. If the optional value is
 * set (check with `.has_value()`) that means the user wants to control this property. Get the value
 * of the optional with the star operator (`*call.get_mode()`) and apply it.
 */
class HumidifierCall {
 public:
  explicit HumidifierCall(Humidifier *parent) : parent_(parent) {}

  /// Set the mode of the humidifier device.
  HumidifierCall &set_mode(HumidifierMode mode);
  /// Set the mode of the humidifier device.
  HumidifierCall &set_mode(optional<HumidifierMode> mode);
  /// Set the mode of the humidifier device based on a string.
  HumidifierCall &set_mode(const std::string &mode);
  /// Set the target humidity of the humidifier device.
  HumidifierCall &set_target_humidity(float target_humidity);
  /// Set the target humidity of the humidifier device.
  HumidifierCall &set_target_humidity(optional<float> target_humidity);

  void perform();

  const optional<HumidifierMode> &get_mode() const;
  const optional<float> &get_target_humidity() const;

 protected:
  void validate_();

  Humidifier *const parent_;
  optional<HumidifierMode> mode_;
  optional<float> target_humidity_;
};

/// Struct used to save the state of the humidifier device in restore memory.
/// Make sure to update RESTORE_STATE_VERSION when changing the struct entries.
struct HumidifierDeviceRestoreState {
  HumidifierMode mode;
  float target_humidity;

  /// Convert this struct to a humidifier call that can be performed.
  HumidifierCall to_call(Humidifier *humidifier);
  /// Apply these settings to the humidifier device.
  void apply(Humidifier *humidifier);
} __attribute__((packed));

/**
 * HumidifierDevice - This is the base class for all humidifier integrations. Each integration
 * needs to extend this class and implement two functions:
 *
 *  - get_traits() - return the static traits of the humidifier device
 *  - control(HumidifierDeviceCall call) - Apply the given changes from call.
 *
 * To write data to the frontend, the integration must first set the properties using
 * this->property = value; (for example this->current_humidity = 55;); then the integration
 * must call this->publish_state(); to send the entire state to the frontend.
 *
 * The entire state of the humifier device is encoded in public properties of the base class (current_humidity,
 * mode etc). These are read-only for the user and rw for integrations. The reason these are public
 * is for simple access to them from lambdas `if (id(my_humifier).mode == humidifier::HUMIDIFIER_MODE_ON) ...`
 */
class Humidifier : public EntityBase {
 public:
  Humidifier() {}

  /// The active mode of the humidifier device.
  HumidifierMode mode{HUMIDIFIER_MODE_OFF};

  /// The active state of the humidifier device.
  HumidifierAction action{HUMIDIFIER_ACTION_OFF};

  /// The current humidity of the humidifier device, as reported from the integration.
  float current_humidity{NAN};

  union {
    /// The target humidity of the humidifier device.
    float target_humidity;
  };

  /** Add a callback for the humidifier device state, each time the state of the humidifier device is updated
   * (using publish_state), this callback will be called.
   *
   * @param callback The callback to call.
   */
  void add_on_state_callback(std::function<void(Humidifier &)> &&callback);

  /**
   * Add a callback for the humidifier device configuration; each time the configuration parameters of a humidifier
   * device is updated (using perform() of a HumidifierCall), this callback will be called, before any on_state
   * callback.
   *
   * @param callback The callback to call.
   */
  void add_on_control_callback(std::function<void(HumidifierCall &)> &&callback);

  /** Make a humidifier device control call, this is used to control the humidifier device, see the HumidifierCall
   * description for more info.
   * @return A new HumidifierCall instance targeting this humidifier device.
   */
  HumidifierCall make_call();

  /** Publish the state of the humidifier device, to be called from integrations.
   *
   * This will schedule the humidifier device to publish its state to all listeners and save the current state
   * to recover memory.
   */
  void publish_state();

  /** Get the traits of this humidifier device with all overrides applied.
   *
   * Traits are static data that encode the capabilities and static data for a humidifier device such as supported
   * modes,humidity  range etc.
   */
  HumidifierTraits get_traits();

  void set_visual_min_humidity_override(float visual_min_humidity_override);
  void set_visual_max_humidity_override(float visual_max_humidity_override);
  void set_visual_humidity_step_override(float target, float current);

 protected:
  friend HumidifierCall;

  /** Get the default traits of this humidifier device.
   *
   * Traits are static data that encode the capabilities and static data for a humidifier device such as supported
   * modes, temperature range etc. Each integration must implement this method and the return value must
   * be constant during all of execution time.
   */
  virtual HumidifierTraits traits() = 0;

  /** Control the humidifier device, this is a virtual method that each humidifier integration must implement.
   *
   * See more info in HummidifierCall. The integration should check all of its values in this method and
   * set them accordingly. At the end of the call, the integration must call `publish_state()` to
   * notify the frontend of a changed state.
   *
   * @param call The HumidifierCall instance encoding all attribute changes.
   */
  virtual void control(const HumidifierCall &call) = 0;
  /// Restore the state of the humidifier device, call this from your setup() method.
  optional<HumidifierDeviceRestoreState> restore_state_();
  /** Internal method to save the state of the humidifier device to recover memory. This is automatically
   * called from publish_state()
   */
  void save_state_();

  void dump_traits_(const char *tag);

  CallbackManager<void(Humidifier &)> state_callback_{};
  CallbackManager<void(HumidifierCall &)> control_callback_{};
  ESPPreferenceObject rtc_;
  optional<float> visual_min_humidity_override_{};
  optional<float> visual_max_humidity_override_{};
  optional<float> visual_target_humidity_step_override_{};
  optional<float> visual_current_humidity_step_override_{};
};

}  // namespace humidifier
}  // namespace esphome