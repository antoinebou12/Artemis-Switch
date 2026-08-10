#include "DisplayTransformStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::video {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_display.json";
}

bool dumpAtomically(json_t* root) {
    const auto destination = storePath();
    const auto temporary = destination.string() + ".tmp";
    if (json_dump_file(root, temporary.c_str(), JSON_INDENT(4) | JSON_SORT_KEYS) != 0)
        return false;
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error) return true;
    const auto backup = destination.string() + ".bak";
    std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(destination))
        std::filesystem::rename(destination, backup, error);
    if (error) return false;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::rename(backup, destination, ignored);
        return false;
    }
    std::filesystem::remove(backup, error);
    return true;
}
}

DisplayTransformStore& DisplayTransformStore::instance() {
    static DisplayTransformStore store;
    return store;
}

void DisplayTransformStore::ensureLoaded() {
    if (!m_loaded) reload();
}

DisplayTransform DisplayTransformStore::get() {
    ensureLoaded();
    return m_transform;
}

void DisplayTransformStore::set(DisplayTransform transform) {
    m_transform = validateDisplayTransform(transform);
    m_loaded = true;
    save();
}

void DisplayTransformStore::setRotation(Rotation rotation) {
    auto value = get();
    value.rotation = rotation;
    set(value);
}

void DisplayTransformStore::reload() {
    m_transform = {};
    m_loaded = true;
    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!json_is_object(root)) {
        if (root) json_decref(root);
        return;
    }
    const json_t* version = json_object_get(root, "schema_version");
    if (!json_is_integer(version) ||
        json_integer_value(version) != SchemaVersion) {
        json_decref(root);
        return;
    }
    if (json_t* value = json_object_get(root, "rotation"); json_is_integer(value)) {
        switch (json_integer_value(value)) {
        case 90: m_transform.rotation = Rotation::Deg90; break;
        case 180: m_transform.rotation = Rotation::Deg180; break;
        case 270: m_transform.rotation = Rotation::Deg270; break;
        default: m_transform.rotation = Rotation::Deg0; break;
        }
    }
    if (json_t* value = json_object_get(root, "zoom"); json_is_number(value))
        m_transform.zoom = static_cast<float>(json_number_value(value));
    if (json_t* value = json_object_get(root, "pan_x"); json_is_number(value))
        m_transform.panX = static_cast<float>(json_number_value(value));
    if (json_t* value = json_object_get(root, "pan_y"); json_is_number(value))
        m_transform.panY = static_cast<float>(json_number_value(value));
    m_transform = validateDisplayTransform(m_transform);
    json_decref(root);
}

bool DisplayTransformStore::save() const {
    json_t* root = json_object();
    if (!root) return false;
    const auto value = validateDisplayTransform(m_transform);
    json_object_set_new(root, "schema_version", json_integer(SchemaVersion));
    json_object_set_new(root, "rotation",
                        json_integer(static_cast<int>(value.rotation)));
    json_object_set_new(root, "zoom", json_real(value.zoom));
    json_object_set_new(root, "pan_x", json_real(value.panX));
    json_object_set_new(root, "pan_y", json_real(value.panY));
    const bool result = dumpAtomically(root);
    json_decref(root);
    return result;
}

} // namespace artemis::video
