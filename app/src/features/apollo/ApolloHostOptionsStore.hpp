#pragma once

#include "ApolloHostOptions.hpp"

#include <map>
#include <string>

namespace artemis::apollo {

class ApolloHostOptionsStore {
public:
    static constexpr int SchemaVersion = 2;
    static ApolloHostOptionsStore& instance();

    ApolloHostOptions get(const std::string& hostKey);
    void set(const std::string& hostKey, ApolloHostOptions options);
    void reload();
    bool save() const;

private:
    void ensureLoaded();
    std::map<std::string, ApolloHostOptions> m_hosts;
    bool m_loaded = false;
};

} // namespace artemis::apollo
