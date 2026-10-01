//
//  add_host_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <borealis.hpp>
#include "Settings.hpp"
#include "GameStreamClient.hpp"
#include "remote_access/IRemoteAccessProvider.hpp"

#include <atomic>
#include <memory>

class AddHostTab : public brls::Box
{
  public:
    AddHostTab();
    ~AddHostTab() override;

    static brls::View* create();

  private:
    void findHost();
    void stopSearchHost();
    void connectHost(const Host& host);
    void fillSearchBox(const GSResult<std::vector<Host>>& hostsRes);
    void appendSearchHosts(const std::vector<Host>& hosts);
    void refreshExtraEndpointsDetail();
    static void pauseSearching();
    static void startSearching();
    brls::Event<GSResult<std::vector<Host>>>::Subscription searchSubscription;
    uint64_t searchGeneration = 0;
    std::vector<std::string> extraEndpoints;

    bool searchBoxIpExists(const std::string& ip);

    // Adds streamable peers from the active remote-access provider to the
    // search results. Probing each peer costs a network round trip, so the work
    // happens on a worker thread; `alive` guards the hop back.
    void appendRemoteAccessPeers();
    // "Add host on 192.168.1.0/24 (via router)" row for a shared LAN subnet.
    void appendSubnetShortcut(const std::string& subnet,
                              const std::string& inputPrefix,
                              const std::string& viaName);
    // Connects to an address typed by the user (manual box or subnet row).
    void connectTypedAddress(const std::string& inputAddress);
    // Tailscale: show only peers that run a GameStream server, plus subnet
    // shortcuts. Runs on a worker thread; probes peers one at a time.
    void appendFilteredPeers(
        const std::vector<RemoteAccessPeer>& peers,
        const std::string& providerName, std::uint64_t generation,
        std::shared_ptr<std::atomic<std::uint64_t>> probeGen,
        std::shared_ptr<std::atomic<bool>> guard);

    // Cleared in the destructor so an in-flight peer probe cannot touch the
    // view after it is gone.
    std::shared_ptr<std::atomic<bool>> alive =
        std::make_shared<std::atomic<bool>>(true);
    // Bumped by every new search, every connect and the destructor. A peer
    // probe that sees a different value stops and drops its result, so it
    // never overlaps with connecting to a host.
    std::shared_ptr<std::atomic<std::uint64_t>> probeGeneration =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    
    BRLS_BIND(brls::InputCell, hostIP, "hostIP");
    BRLS_BIND(brls::DetailCell, addEndpoint, "add_endpoint");
    BRLS_BIND(brls::DetailCell, connect, "connect");
    BRLS_BIND(brls::Box, searchBox, "search_box");
    BRLS_BIND(brls::Box, loader, "loader");
    BRLS_BIND(brls::Header, searchHeader, "search_header");
};
