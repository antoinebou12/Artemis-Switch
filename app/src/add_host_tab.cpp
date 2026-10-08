//
//  add_host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "add_host_tab.hpp"

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
#include "remote_access/AddHostPeerFilter.hpp"
#include "remote_access/RemoteAccessManager.hpp"
#include "remote_access/RemoteRouting.hpp"
#include "remote_access_provider_id.hpp"
#include "http.h"
#include "vpn/VpnFileLogger.hpp"
#include <chrono>

namespace {
// Probe results outlive the screen, so reopening Add Host is instant.
artemis::remote::ProbeCache& peerProbeCache() {
    static artemis::remote::ProbeCache cache;
    return cache;
}
} // namespace
#endif
#include "DiscoverManager.hpp"
#include "NetBirdManager.hpp"
#include "helper.hpp"
#include "main_tabs_view.hpp"
#include "features/host/HostAddressParse.hpp"

#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
extern void darwin_mdns_start(ServerCallback<std::vector<Host>>& callback);
extern void darwin_mdns_stop();
#endif

AddHostTab::AddHostTab() {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/add_host.xml");

    hostIP->init("add_host/host_ip"_i18n, "");
    hostIP->setPlaceholder("stream.example.com:47989");
    hostIP->setHint("192.168.1.109:47989");

    addEndpoint->setText("host/add_endpoint"_i18n);
    refreshExtraEndpointsDetail();
    addEndpoint->registerClickAction([this](View* view) {
        Application::getPlatform()->getImeManager()->openForText(
            [this](const std::string& text) {
                if (text.empty()) {
                    return;
                }
                this->extraEndpoints.push_back(text);
                this->refreshExtraEndpointsDetail();
            },
            "host/add_endpoint_title"_i18n, "", 80, "", 0);
        return true;
    });

    connect->setText("add_host/connect"_i18n);
    connect->registerClickAction([this](View* view) {
        connectTypedAddress(hostIP->getValue());
        return true;
    });

    if (GameStreamClient::can_find_host())
        findHost();
    else {
        searchHeader->setTitle("add_host/search_error"_i18n);
        loader->setVisibility(brls::Visibility::GONE);
    }

    registerAction("add_host/search_refresh"_i18n, ControllerButton::BUTTON_X,
                   [this](View* view) {
#ifdef MULTICAST_DISABLED
                       DiscoverManager::instance().reset();
#endif
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
                       // An explicit Refresh rechecks devices that did not
                       // answer before (a PC that was asleep); found hosts
                       // stay known so the list still fills in fast.
                       peerProbeCache().forgetMisses();
#endif
                       findHost();
                       return true;
                   });
    setActionAvailable(BUTTON_X, GameStreamClient::can_find_host());
}

void AddHostTab::connectTypedAddress(const std::string& inputAddress) {
    Host host;
    const auto parsed = artemis::host::parse_host_address(inputAddress);
    if (parsed.host.empty()) {
        showError("add_host/invalid_address"_i18n);
        return;
    }

    if (artemis::host::should_store_as_remote(parsed)) {
        host.remoteAddress = inputAddress;
    } else {
        host.address = inputAddress;
    }
    host.ensure_endpoints();
    for (const auto& endpoint : extraEndpoints) {
        host.add_endpoint("Custom", endpoint);
    }
    connectHost(host);
}

void AddHostTab::appendSubnetShortcut(const std::string& subnet,
                                      const std::string& inputPrefix,
                                      const std::string& viaName) {
    auto cell = new brls::DetailCell();
    cell->setText(brls::getStr("add_host/subnet_shortcut", subnet));
    cell->setDetailText(brls::getStr("add_host/subnet_via", viaName));
    cell->setDetailTextColor(
        brls::Application::getTheme()["brls/text_disabled"]);
    cell->registerClickAction([this, subnet, inputPrefix](View*) {
        // Hosts behind a subnet router are not tailnet devices, so they cannot
        // be listed; pre-type the subnet so only the last part is needed.
        Application::getPlatform()->getImeManager()->openForText(
            [this](const std::string& text) {
                if (!text.empty())
                    connectTypedAddress(text);
            },
            brls::getStr("add_host/subnet_title", subnet), "", 64, inputPrefix,
            0);
        return true;
    });
    searchBox->addView(cell);
}

void AddHostTab::refreshExtraEndpointsDetail() {
    if (extraEndpoints.empty()) {
        addEndpoint->setDetailText("add_host/extra_endpoints_none"_i18n);
        return;
    }
    addEndpoint->setDetailText("add_host/extra_endpoints_count"_i18n + " (" +
                               std::to_string(extraEndpoints.size()) + ")");
}

void AddHostTab::fillSearchBox(const GSResult<std::vector<Host>>& hostsRes) {
    loader->setVisibility(DiscoverManager::instance().isPaused()
                              ? brls::Visibility::GONE
                              : brls::Visibility::VISIBLE);

    if (hostsRes.isSuccess()) {
        appendSearchHosts(hostsRes.value());
    } else {
        loader->setVisibility(brls::Visibility::GONE);
        searchHeader->setTitle("add_host/search"_i18n + " - " +
                               hostsRes.error());
    }

    // Remote peers do not come from LAN discovery, so they must not be gated on
    // it. DiscoverManager reports failure ("no_host") whenever no LAN host
    // answers, which is the normal case when streaming from away -- precisely
    // when the tunnel peers are the only reachable hosts. Appending them only
    // on the success branch hid them exactly when they mattered most.
    appendRemoteAccessPeers();
}

void AddHostTab::appendSearchHosts(const std::vector<Host>& hosts) {
    for (const Host& host : hosts) {
        const auto displayAddress = host.preferred_address();
        if (displayAddress.empty() || searchBoxIpExists(displayAddress))
            continue;

        auto hostButton = new brls::DetailCell();
        hostButton->setText(host.hostname);
        hostButton->setDetailText(displayAddress);
        hostButton->setDetailTextColor(
            brls::Application::getTheme()["brls/text_disabled"]);
        hostButton->registerClickAction([this, host](View* view) {
            connectHost(host);
            return true;
        });
        searchBox->addView(hostButton);
    }
}

void AddHostTab::appendRemoteAccessPeers() {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
    auto& manager = RemoteAccessManager::instance();
    const auto providerId = manager.activeProviderId();
    if (providerId.empty()) {
        return;
    }

    auto* provider = manager.provider(providerId);
    if (!provider) {
        return;
    }

    // peers() is a cache read. Refresh the authenticated provider directory off
    // the UI thread and only then build the rows.
    const std::string providerName = provider->name();
    const bool filterPeers = provider->id() == "tailscale";
    const std::uint64_t generation = probeGeneration->fetch_add(1) + 1;
    brls::async([this, provider, providerName, filterPeers, generation,
                 probeGen = probeGeneration, guard = alive]() {
        if (!guard->load()) {
            return;
        }
        provider->refreshPeers();
        auto peers = provider->peers();

        if (filterPeers) {
            appendFilteredPeers(peers, providerName, generation, probeGen,
                                guard);
            return;
        }

        brls::sync([this, guard, peers, providerName]() {
            if (!guard->load()) {
                return;
            }

            std::vector<Host> hosts;
            for (const auto& peer : peers) {
                // NetBird cannot test the 100.x GameStream port until its
                // exclusive loopback route is active. Authenticated provider
                // peers are therefore offered here and the normal GameStream
                // handshake performs the final service check.
                if (!peer.online || peer.address.empty()) {
                    continue;
                }

                Host host;
                host.hostname = peer.name.empty() ? peer.address : peer.name;
                // Keep the real mesh address as identity. The tunnel route
                // swaps in loopback at connect time, so the saved host still
                // shows where it actually lives.
                host.address = peer.address;
                HostEndpoint endpoint;
                endpoint.label = providerName;
                endpoint.address = peer.address;
                // Priority 2 keeps LAN (0) and manual remote (1) ahead of the
                // tunnel, so home streaming never detours through the VPN.
                endpoint.priority = 2;
                host.endpoints.push_back(std::move(endpoint));
                hosts.push_back(std::move(host));
            }
            appendSearchHosts(hosts);
        });
    });
#endif
}

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
namespace {

using artemis::remote::PeerVerdict;

constexpr std::size_t kMaxPeerProbes = 16;

enum class ProbeResult { Host, NotHost, Unknown };

void logAddHostFilter(const std::string& message) {
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "Remote", VpnFileLogger::Severity::Info,
                          "add host filter: " + message);
}

// Opens the peer's route for a moment and asks for /serverinfo over plain
// HTTP (no pairing or client certificate needed). Any GameStream server
// (Sunshine, Apollo, Vibepollo, GFE) answers with its <appversion>. The
// route is released when the lease goes out of scope.
ProbeResult probeGameStreamPeer(const std::string& address) {
    auto lease = artemis::remote::acquireRouteFor(address);
    if (!lease.isActive())
        return ProbeResult::Unknown; // route refused: cannot tell
    http_init(Settings::instance().key_dir());
    Data data;
    const std::string url = std::string("http://") +
                            artemis::remote::kProxyAddress + ":" +
                            std::to_string(artemis::remote::kGameStreamHttpPort) +
                            "/serverinfo?uniqueid=0123456789ABCDEF";
    if (http_request(url, &data, HTTPRequestTimeoutMedium) != GS_OK)
        return ProbeResult::NotHost;
    std::string version;
    return xml_search(data, "appversion", &version) == GS_OK && !version.empty()
               ? ProbeResult::Host
               : ProbeResult::NotHost;
}

Host hostFromPeer(const RemoteAccessPeer& peer, const std::string& providerName) {
    Host host;
    // Display label only; pairing replaces it with the server's own name.
    host.hostname = artemis::remote::peerDisplayName(peer) + " · " + providerName;
    host.address = peer.address;
    HostEndpoint endpoint;
    endpoint.label = providerName;
    endpoint.address = peer.address;
    endpoint.priority = 2; // LAN (0) and manual remote (1) stay ahead
    host.endpoints.push_back(std::move(endpoint));
    return host;
}

} // namespace

void AddHostTab::appendFilteredPeers(
    const std::vector<RemoteAccessPeer>& peers, const std::string& providerName,
    std::uint64_t generation,
    std::shared_ptr<std::atomic<std::uint64_t>> probeGen,
    std::shared_ptr<std::atomic<bool>> guard) {
    const auto current = [&] {
        return guard->load() && probeGen->load() == generation;
    };
    const auto appendHosts = [this, guard](std::vector<Host> hosts) {
        if (hosts.empty())
            return;
        brls::sync([this, guard, hosts = std::move(hosts)]() {
            if (guard->load())
                appendSearchHosts(hosts);
        });
    };

    const auto shortcuts = artemis::remote::subnetShortcuts(peers);
    std::vector<Host> ready;
    std::vector<RemoteAccessPeer> toProbe;
    const auto now = artemis::remote::ProbeCache::Clock::now();
    for (const auto& peer : peers) {
        const auto name = artemis::remote::peerDisplayName(peer);
        switch (artemis::remote::classifyPeerForAddHost(peer)) {
        case PeerVerdict::Hidden:
            if (peer.online)
                logAddHostFilter(name + " hidden (" +
                                 (peer.address.empty() ? std::string("no address")
                                                       : peer.os) +
                                 ")");
            break;
        case PeerVerdict::Host:
            logAddHostFilter(name + " shown (reports a GameStream port)");
            ready.push_back(hostFromPeer(peer, providerName));
            break;
        case PeerVerdict::Probe:
            if (const auto cached = peerProbeCache().lookup(peer.peerId, now)) {
                if (*cached)
                    ready.push_back(hostFromPeer(peer, providerName));
            } else {
                toProbe.push_back(peer);
            }
            break;
        }
    }

    brls::sync([this, guard, shortcuts, ready]() {
        if (!guard->load())
            return;
        for (const auto& shortcut : shortcuts)
            appendSubnetShortcut(shortcut.subnet, shortcut.inputPrefix,
                                 shortcut.viaName);
        appendSearchHosts(ready);
    });

    if (toProbe.empty())
        return;
    auto& manager = RemoteAccessManager::instance();
    std::size_t probed = 0;
    for (std::size_t i = 0; i < toProbe.size(); ++i) {
        if (!current())
            return; // left the screen, new search, or connecting to a host
        // Never replace a route that is in use (stream, app list, pairing),
        // and stay bounded on large tailnets. Unchecked peers are shown
        // rather than hidden, so nothing goes missing.
        if (manager.hasActiveRoute() || probed >= kMaxPeerProbes) {
            std::vector<Host> unchecked;
            for (std::size_t j = i; j < toProbe.size(); ++j)
                unchecked.push_back(hostFromPeer(toProbe[j], providerName));
            logAddHostFilter(std::to_string(unchecked.size()) +
                             " peers shown unchecked (" +
                             (probed >= kMaxPeerProbes ? "probe limit reached"
                                                       : "a host is in use") +
                             ")");
            appendHosts(std::move(unchecked));
            return;
        }
        const auto& peer = toProbe[i];
        const auto name = artemis::remote::peerDisplayName(peer);
        ++probed;
        const auto started = std::chrono::steady_clock::now();
        const auto result = probeGameStreamPeer(peer.address);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - started)
                            .count();
        // A connect that started meanwhile replaces the probe's route, which
        // makes the probe fail: that answer says nothing about the peer.
        if (!current())
            return;
        switch (result) {
        case ProbeResult::Host:
            peerProbeCache().store(peer.peerId, true, now);
            logAddHostFilter(name + " shown (GameStream answered in " +
                             std::to_string(ms) + " ms)");
            appendHosts({hostFromPeer(peer, providerName)});
            break;
        case ProbeResult::NotHost:
            peerProbeCache().store(peer.peerId, false, now);
            logAddHostFilter(name + " hidden (no GameStream server, " +
                             std::to_string(ms) + " ms)");
            break;
        case ProbeResult::Unknown:
            logAddHostFilter(name + " shown unchecked (route refused)");
            appendHosts({hostFromPeer(peer, providerName)});
            break;
        }
    }
}
#else
void AddHostTab::appendFilteredPeers(
    const std::vector<RemoteAccessPeer>&, const std::string&, std::uint64_t,
    std::shared_ptr<std::atomic<std::uint64_t>>,
    std::shared_ptr<std::atomic<bool>>) {}
#endif

bool AddHostTab::searchBoxIpExists(const std::string& ip) {
    return std::any_of(searchBox->getChildren().begin(), searchBox->getChildren().end(), [ip](View* child) {
        auto cell = dynamic_cast<DetailCell*>(child);
        return cell->detail->getFullText() == ip;
    });
}

void AddHostTab::findHost() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().start();
    fillSearchBox(DiscoverManager::instance().getHosts());
    DiscoverManager::instance().getHostsUpdateEvent()->unsubscribe(
        searchSubscription);
    searchSubscription =
        DiscoverManager::instance().getHostsUpdateEvent()->subscribe(
            [this](auto result) { fillSearchBox(result); });
#else
    stopSearchHost();
    const uint64_t generation = searchGeneration;
    searchBox->clearViews();
    searchHeader->setTitle("add_host/search"_i18n);
    loader->setVisibility(brls::Visibility::VISIBLE);

#if defined(__SWITCH__)
    // NetBird login/sync can block while the relay is established, so discover
    // mesh peers off the UI thread. Each result keeps the real 100.x address as
    // a named endpoint. NetBirdManager's gs_init wrapper transparently starts
    // the localhost TCP/UDP proxy when the user connects to that peer.
    {
        ASYNC_RETAIN
        brls::async([ASYNC_TOKEN, generation] {
            std::vector<Host> netbirdHosts;
            for (const auto& peer : NetBirdManager::instance().peers()) {
                Host host;
                host.address = peer.address;
                host.hostname = peer.name;
                host.endpoints.push_back({"NetBird", peer.address, 10});
                netbirdHosts.push_back(std::move(host));
            }

            brls::sync([ASYNC_TOKEN, generation,
                        netbirdHosts = std::move(netbirdHosts)]() mutable {
                ASYNC_RELEASE
                if (generation != searchGeneration) {
                    return;
                }
                appendSearchHosts(netbirdHosts);
            });
        });
    }

    // The provider-based directory covers every configured remote-access stack
    // (NetBird, WireGuard, Tailscale) and drops peers whose authenticated
    // identity does not check out. Its only other call site sits behind
    // MULTICAST_DISABLED, which is set for PS Vita alone, so on Switch it never
    // ran and WireGuard/Tailscale peers were never offered here.
    //
    // This runs alongside the NetBird block above rather than replacing it: the
    // legacy path calls ensure_connected() and so brings the tunnel up on
    // demand, whereas the provider cache is only populated once a provider is
    // started and ready. appendSearchHosts() skips addresses already listed, so
    // whichever resolves first wins and the other is deduplicated.
    appendRemoteAccessPeers();
#endif

    ASYNC_RETAIN
#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
    darwin_mdns_start(
#else
    GameStreamClient::find_hosts(
#endif
        [ASYNC_TOKEN, generation](const GSResult<std::vector<Host>>& result) {
            ASYNC_RELEASE

            if (generation != searchGeneration) {
                return;
            }

            if (result.isSuccess()) {
                appendSearchHosts(result.value());
            } else {
                loader->setVisibility(brls::Visibility::GONE);
                showError(result.error(), [] {});
            }
        });
#endif
}

void AddHostTab::stopSearchHost() {
    searchGeneration++;
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
#elif defined(PLATFORM_IOS)
#elif defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
#else
    GameStreamClient::cancel_find_hosts();
#endif
}

namespace {
// Saves a newly paired host and records the outcome in vpn.log on VPN builds,
// so a host that fails to reach the list is visible instead of silent.
void savePairedHost(const Host& host) {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
    const size_t before = Settings::instance().hosts().size();
#endif
    Settings::instance().add_host(host);
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD) || defined(ENABLE_TAILSCALE))
    const auto saved = Settings::instance().hosts();
    const bool found = std::any_of(saved.begin(), saved.end(),
                                   [&host](const Host& candidate) {
                                       return hosts_match(candidate, host);
                                   });
    artemis::remote::logHostSaveResult(host.hostname, host.preferred_address(),
                                       host.mac, before, saved.size(), found);
#endif
}
} // namespace

void AddHostTab::connectHost(const Host& host) {
    // A running peer probe must not hold or replace the route this connect
    // is about to use.
    probeGeneration->fetch_add(1);
    pauseSearching();

    Dialog* loaderView = createLoadingDialog("add_host/try_connect"_i18n);
    loaderView->open();

    GameStreamClient::instance().connect(
        host, [this, loaderView, host](const GSResult<SERVER_DATA>& result) {
            loaderView->close([this, result, host] {
                if (result.isSuccess()) {
                    Host pairedHost = host;
                    pairedHost.hostname = result.value().hostname;
                    pairedHost.mac = result.value().mac;

                    if (result.value().paired) {
                        showAlert("add_host/paired_error"_i18n, [pairedHost] {
                            savePairedHost(pairedHost);
                            MainTabs::getInstanse()->refillTabs();
                        });

                        return;
                    }

                    auto pin = fmt::format("{}{}{}{}", (int)rand() % 10, (int)rand() % 10,
                            (int)rand() % 10, (int)rand() % 10);

                    const CancellationToken cancellation;
                    const auto dialogDismissed =
                        std::make_shared<std::atomic_bool>(false);
                    brls::Dialog* dialog = createLoadingDialog(
                        "add_host/pair_prefix"_i18n + pin +
                            "add_host/pair_postfix"_i18n,
                        [cancellation, dialogDismissed] {
                            dialogDismissed->store(true);
                            (void)cancellation.cancel();
                        });
                    dialog->open();

                    ASYNC_RETAIN
                    GameStreamClient::instance().pair(
                        pairedHost, pin,
                        [ASYNC_TOKEN, pairedHost, dialog, cancellation,
                         dialogDismissed](const GSResult<bool>& result) {
                            ASYNC_RELEASE
                            if (cancellation.isCancellationRequested()) {
                                AddHostTab::startSearching();
                                return;
                            }
                            const auto finish = [result, pairedHost] {
                                if (result.isSuccess()) {
                                    savePairedHost(pairedHost);
                                    MainTabs::getInstanse()->refillTabs();
                                    AddHostTab::startSearching();
                                } else {
                                    showError(result.error(), [] {
                                        AddHostTab::startSearching();
                                    });
                                }
                            };
                            // A late Cancel press can dismiss the dialog after
                            // the worker has already committed completion. The
                            // result is still valid, but the pointer is not.
                            if (dialogDismissed->load()) {
                                finish();
                            } else {
                                dialog->dismiss(finish);
                            }
                        }, cancellation);
                } else {
                    showError(result.error(),
                              [] { AddHostTab::startSearching(); });
                }
            });
        });
}

void AddHostTab::pauseSearching() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
#endif
}

void AddHostTab::startSearching() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().start();
#endif
}

AddHostTab::~AddHostTab() {
    // An in-flight peer probe must not append rows to a destroyed view.
    alive->store(false);
    probeGeneration->fetch_add(1);
    stopSearchHost();
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
    DiscoverManager::instance().getHostsUpdateEvent()->unsubscribe(
        searchSubscription);
#elif defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
    darwin_mdns_stop();
#endif
}

brls::View* AddHostTab::create() {
    // Called by the XML engine to create a new AddHostTab
    return new AddHostTab();
}
