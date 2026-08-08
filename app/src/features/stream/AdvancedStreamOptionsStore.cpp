#include "AdvancedStreamOptionsStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::stream {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_advanced_stream.json";
}
}

AdvancedStreamOptionsStore& AdvancedStreamOptionsStore::instance() {
    static AdvancedStreamOptionsStore store;
    return store;
}

void AdvancedStreamOptionsStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

const AdvancedStreamOptions& AdvancedStreamOptionsStore::get() {
    ensureLoaded();
    return m_options;
}

void AdvancedStreamOptionsStore::set(const AdvancedStreamOptions& options) {
    m_options = options;
    m_loaded = true;
    save();
}

void AdvancedStreamOptionsStore::reload() {
    m_options = {};
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        return;
    }

    if (json_t* value = json_object_get(root, "unlock_all_frame_rates"))
        m_options.unlockAllFrameRates = json_is_true(value);
    if (json_t* value = json_object_get(root, "force_full_range_video"))
        m_options.forceFullRangeVideo = json_is_true(value);
    if (json_t* value = json_object_get(root, "prevent_packet_loss"))
        m_options.preventPacketLoss = json_is_true(value);

    json_decref(root);
}

bool AdvancedStreamOptionsStore::save() const {
    json_t* root = json_object();
    if (!root)
        return false;

    json_object_set_new(root, "unlock_all_frame_rates",
                        m_options.unlockAllFrameRates ? json_true() : json_false());
    json_object_set_new(root, "force_full_range_video",
                        m_options.forceFullRangeVideo ? json_true() : json_false());
    json_object_set_new(root, "prevent_packet_loss",
                        m_options.preventPacketLoss ? json_true() : json_false());

    const int result = json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::stream
