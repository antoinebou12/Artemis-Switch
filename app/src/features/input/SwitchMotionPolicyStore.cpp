#include "SwitchMotionPolicyStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::input {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_motion.json";
}
}

SwitchMotionPolicyStore& SwitchMotionPolicyStore::instance() {
    static SwitchMotionPolicyStore store;
    return store;
}

void SwitchMotionPolicyStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

const SwitchMotionOptions& SwitchMotionPolicyStore::get() {
    ensureLoaded();
    return m_options;
}

void SwitchMotionPolicyStore::set(const SwitchMotionOptions& options,
                                  bool persist) {
    m_options = options;
    m_loaded = true;
    if (persist)
        save();
}

void SwitchMotionPolicyStore::reload() {
    m_options = {};
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root) json_decref(root);
        return;
    }

    if (json_t* value = json_object_get(root, "allow_gamepad_motion_sensors"))
        m_options.allowGamepadMotionSensors = json_is_true(value);
    if (json_t* value = json_object_get(root, "allow_console_motion_fallback"))
        m_options.allowConsoleMotionFallback = json_is_true(value);

    json_decref(root);
}

bool SwitchMotionPolicyStore::save() const {
    json_t* root = json_object();
    if (!root) return false;

    json_object_set_new(root, "allow_gamepad_motion_sensors",
                        m_options.allowGamepadMotionSensors ? json_true() : json_false());
    json_object_set_new(root, "allow_console_motion_fallback",
                        m_options.allowConsoleMotionFallback ? json_true() : json_false());

    const int result = json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::input
