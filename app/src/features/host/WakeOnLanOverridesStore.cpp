#include "WakeOnLanOverridesStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::host {
namespace {

std::filesystem::path path() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_wol_overrides.json";
}

bool dumpAtomically(json_t* root) {
    const auto destination = path();
    const auto temporary = destination.string() + ".tmp";
    if (json_dump_file(root, temporary.c_str(),
                       JSON_INDENT(4) | JSON_SORT_KEYS) != 0)
        return false;
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error)
        return true;
    const auto backup = destination.string() + ".bak";
    std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(destination))
        std::filesystem::rename(destination, backup, error);
    if (error)
        return false;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::rename(backup, destination, ignored);
        return false;
    }
    std::filesystem::remove(backup, error);
    return true;
}

void readStringField(json_t* item, const char* name, std::string& target) {
    if (json_t* v = json_object_get(item, name); json_is_string(v))
        target = json_string_value(v);
}

} // namespace

WakeOnLanOverridesStore& WakeOnLanOverridesStore::instance() {
    static WakeOnLanOverridesStore store;
    return store;
}

void WakeOnLanOverridesStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

WakeOnLanOverride WakeOnLanOverridesStore::get(const std::string& hostKey) {
    ensureLoaded();
    if (const auto it = m_overrides.find(hostKey); it != m_overrides.end())
        return it->second;
    return {};
}

void WakeOnLanOverridesStore::set(const std::string& hostKey,
                                  WakeOnLanOverride over) {
    ensureLoaded();
    if (!hostKey.empty())
        m_overrides[hostKey] = over;
    save();
}

void WakeOnLanOverridesStore::clear(const std::string& hostKey) {
    ensureLoaded();
    const auto erased = m_overrides.erase(hostKey) > 0;
    if (erased)
        save();
}

void WakeOnLanOverridesStore::reload() {
    m_overrides.clear();
    m_loaded = true;
    json_error_t error{};
    json_t* root = json_load_file(path().string().c_str(), 0, &error);
    if (!json_is_object(root)) {
        if (root)
            json_decref(root);
        return;
    }
    json_t* version = json_object_get(root, "schema_version");
    json_t* hosts = json_object_get(root, "hosts");
    if (!json_is_integer(version) ||
        json_integer_value(version) != SchemaVersion ||
        !json_is_object(hosts)) {
        json_decref(root);
        return;
    }

    const char* key;
    json_t* item;
    json_object_foreach(hosts, key, item) {
        if (!json_is_object(item))
            continue;
        WakeOnLanOverride over;
        readStringField(item, "mac", over.mac);
        readStringField(item, "address", over.address);
        readStringField(item, "secure_on_password", over.secureOnPassword);
        if (json_t* port = json_object_get(item, "port"); json_is_integer(port))
            over.port = static_cast<unsigned short>(json_integer_value(port));
        if (json_t* attempts = json_object_get(item, "resend_attempts");
            json_is_integer(attempts))
            over.resendAttempts = json_integer_value(attempts);
        m_overrides[key] = over;
    }
    json_decref(root);
}

bool WakeOnLanOverridesStore::save() const {
    json_t* root = json_object();
    json_t* hosts = json_object();
    if (!root || !hosts) {
        if (root)
            json_decref(root);
        if (hosts)
            json_decref(hosts);
        return false;
    }
    json_object_set_new(root, "schema_version",
                        json_integer(SchemaVersion));
    for (const auto& [key, over] : m_overrides) {
        json_t* item = json_object();
        json_object_set_new(item, "mac",
                            json_string(over.mac.c_str()));
        json_object_set_new(item, "address",
                            json_string(over.address.c_str()));
        json_object_set_new(item, "port",
                            json_integer(over.port));
        json_object_set_new(item, "resend_attempts",
                            json_integer(over.resendAttempts));
        json_object_set_new(item, "secure_on_password",
                            json_string(over.secureOnPassword.c_str()));
        json_object_set_new(hosts, key.c_str(), item);
    }
    json_object_set_new(root, "hosts", hosts);
    const bool result = dumpAtomically(root);
    json_decref(root);
    return result;
}

} // namespace artemis::host