#include "ApolloHostOptionsStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::apollo {
namespace {
std::filesystem::path path() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_apollo_hosts.json";
}

bool dumpAtomically(json_t* root) {
    const auto destination = path();
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

ApolloHostOptionsStore& ApolloHostOptionsStore::instance() {
    static ApolloHostOptionsStore store;
    return store;
}

void ApolloHostOptionsStore::ensureLoaded() { if (!m_loaded) reload(); }

ApolloHostOptions ApolloHostOptionsStore::get(const std::string& hostKey) {
    ensureLoaded();
    if (const auto it = m_hosts.find(hostKey); it != m_hosts.end()) return it->second;
    return {};
}

void ApolloHostOptionsStore::set(const std::string& hostKey,
                                 ApolloHostOptions options) {
    ensureLoaded();
    if (!hostKey.empty()) m_hosts[hostKey] = validateApolloHostOptions(options);
    save();
}

void ApolloHostOptionsStore::reload() {
    m_hosts.clear();
    m_loaded = true;
    json_error_t error{};
    json_t* root = json_load_file(path().string().c_str(), 0, &error);
    if (!json_is_object(root)) { if (root) json_decref(root); return; }
    json_t* version = json_object_get(root, "schema_version");
    json_t* hosts = json_object_get(root, "hosts");
    if (!json_is_integer(version) || json_integer_value(version) != SchemaVersion ||
        !json_is_object(hosts)) { json_decref(root); return; }

    const char* key;
    json_t* item;
    json_object_foreach(hosts, key, item) {
        if (!json_is_object(item)) continue;
        ApolloHostOptions options;
        if (json_t* target = json_object_get(item, "target"); json_is_string(target))
            virtualDisplayTargetFromName(json_string_value(target), options.target);
        if (json_t* value = json_object_get(item, "width"); json_is_integer(value))
            options.customWidth = json_integer_value(value);
        if (json_t* value = json_object_get(item, "height"); json_is_integer(value))
            options.customHeight = json_integer_value(value);
        if (json_t* value = json_object_get(item, "refresh_rate"); json_is_integer(value))
            options.refreshRate = json_integer_value(value);
        if (json_t* value = json_object_get(item, "scale_factor"); json_is_integer(value))
            options.scaleFactor = json_integer_value(value);
        m_hosts[key] = validateApolloHostOptions(options);
    }
    json_decref(root);
}

bool ApolloHostOptionsStore::save() const {
    json_t* root = json_object();
    json_t* hosts = json_object();
    if (!root || !hosts) { if (root) json_decref(root); if (hosts) json_decref(hosts); return false; }
    json_object_set_new(root, "schema_version", json_integer(SchemaVersion));
    for (const auto& [key, raw] : m_hosts) {
        const auto options = validateApolloHostOptions(raw);
        json_t* item = json_object();
        json_object_set_new(item, "target", json_string(virtualDisplayTargetName(options.target)));
        json_object_set_new(item, "width", json_integer(options.customWidth));
        json_object_set_new(item, "height", json_integer(options.customHeight));
        json_object_set_new(item, "refresh_rate", json_integer(options.refreshRate));
        json_object_set_new(item, "scale_factor", json_integer(options.scaleFactor));
        json_object_set_new(hosts, key.c_str(), item);
    }
    json_object_set_new(root, "hosts", hosts);
    const bool result = dumpAtomically(root);
    json_decref(root);
    return result;
}

} // namespace artemis::apollo
