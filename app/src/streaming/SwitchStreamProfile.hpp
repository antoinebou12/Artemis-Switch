#pragma once

#include <string>
#include <vector>

namespace artemis::streaming {

struct StreamProfile {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
    int decoderThreads = 2;
    std::string codec = "HEVC";
};

struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
};

class SwitchStreamProfile {
public:
    static ValidationResult validate(const StreamProfile& profile);
    static StreamProfile normalized(StreamProfile profile);
};

} // namespace artemis::streaming
