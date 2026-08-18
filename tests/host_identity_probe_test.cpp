#include "HostIdentityProbe.hpp"
#include "http.h"
#include "errors.h"

#include <cassert>
#include <cstdlib>
#include <string>

Data::Data(size_t capacity) : m_bytes(nullptr), m_size(capacity) {
    if (capacity != 0) {
        m_bytes = static_cast<unsigned char*>(std::calloc(capacity + 1, 1));
    }
}

Data::~Data() {
    std::free(m_bytes);
}

// Parser/URL unit tests do not perform network I/O. This satisfies the probe
// translation unit's transport dependency without linking libcurl/Borealis.
int http_request(const std::string&, Data*, HTTPRequestTimeout,
                 const HTTPRequestOptions&, HTTPResponseInfo*) {
    return GS_FAILED;
}

int main() {
    using namespace artemis::host;

    const auto punktfunk = parsePunktfunkHealth(
        R"({"status":"ok","version":"0.21.0","abi_version":7})");
    assert(punktfunk);
    assert(punktfunk->kind == HostKind::Punktfunk);
    assert(punktfunk->product == "Punktfunk");
    assert(punktfunk->version == "0.21.0");
    assert(punktfunk->webConsolePort == 47992);
    assert(!parsePunktfunkHealth(
        R"({"status":"down","version":"0.21.0","abi_version":7})"));
    assert(!parsePunktfunkHealth(R"({"status":"ok","version":"0.21.0"})"));
    assert(!parsePunktfunkHealth(
        R"({"status":"ok","version":"0.21.0","abi_version":7} trailing)"));
    assert(!parsePunktfunkHealth(
        R"({"status":"ok","version":"0.21.0","abi_version":7broken})"));
    assert(!parsePunktfunkHealth(R"({"name":"unrelated-host"})"));
    assert(!parsePunktfunkHealth("not json"));

    const auto vibeshine = parseVibeshineWebUi(
        "<!doctype html><title>Vibeshine</title>");
    assert(vibeshine);
    assert(vibeshine->kind == HostKind::Vibeshine);
    assert(!parseVibeshineWebUi("<title>Sunshine</title>"));
    assert(!parseVibeshineWebUi("<p>Vibeshine is mentioned here</p>"));

    assert(hostConsoleUrl("192.168.1.5", *vibeshine) ==
           "https://192.168.1.5:47990/");
    assert(hostConsoleUrl("stream.example.com", *punktfunk) ==
           "https://stream.example.com:47992/");
    assert(hostConsoleUrl("stream.example.com:50000", *punktfunk) ==
           "https://stream.example.com:47992/");
    assert(hostConsoleUrl("[2001:db8::1]:47989", *punktfunk) ==
           "https://[2001:db8::1]:47992/");
    assert(std::string(punktfunkGameStreamRequiredError()).find(
               "Enable GameStream") != std::string::npos);
    assert(!probePunktfunkIdentity("probe-times-out.example"));

    return 0;
}
