//
//  add_host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "add_host_tab.hpp"

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
#include "remote_access/RemoteAccessManager.hpp"
#include "remote_access_provider_id.hpp"
#endif
#include "DiscoverManager.hpp"
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
        Host host;
        const auto inputAddress = hostIP->getValue();
        const auto parsed = artemis::host::parse_host_address(inputAddress);
        if (parsed.host.empty()) {
            showError("add_host/invalid_address"_i18n);
            return true;
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
                       findHost();
                       return true;
                   });
    setActionAvailable(BUTTON_X, GameStreamClient::can_find_host());
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
        appendRemoteAccessPeers();
    } else {
        loader->setVisibility(brls::Visibility::GONE);
        searchHeader->setTitle("add_host/search"_i18n + " - " +
                               hostsRes.error());
    }
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
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    auto& manager = RemoteAccessManager::instance();
    const auto providerId = manager.activeProviderId();
    if (providerId.empty()) {
        return;
    }

    auto* provider = manager.provider(providerId);
    if (!provider) {
        return;
    }

    // peers() is a cache read. Refreshing it costs one probe per peer, so do
    // that off the UI thread and only then build the rows.
    const std::string providerName = provider->name();
    brls::async([this, provider, providerName, guard = alive]() {
        if (!guard->load()) {
            return;
        }
        provider->refreshPeers();
        auto peers = provider->peers();

        brls::sync([this, guard, peers, providerName]() {
            if (!guard->load()) {
                return;
            }

            std::vector<Host> hosts;
            for (const auto& peer : peers) {
                // Everything on the mesh answers ping; only things running
                // Sunshine/Apollo answer on the GameStream port. Offering a NAS
                // as a streaming host would just produce a confusing failure.
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

void AddHostTab::connectHost(const Host& host) {
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
                            Settings::instance().add_host(pairedHost);
                            MainTabs::getInstanse()->refillTabs();
                        });

                        return;
                    }

                    auto pin = fmt::format("{}{}{}{}", (int)rand() % 10, (int)rand() % 10,
                            (int)rand() % 10, (int)rand() % 10);

                    brls::Dialog* dialog = createLoadingDialog(
                        "add_host/pair_prefix"_i18n + pin +
                        "add_host/pair_postfix"_i18n);
                    dialog->setCancelable(false);
                    dialog->open();

                    ASYNC_RETAIN
                    GameStreamClient::instance().pair(
                        pairedHost, pin,
                        [ASYNC_TOKEN, pairedHost, dialog](const GSResult<bool>& result) {
                            ASYNC_RELEASE
                            dialog->dismiss([result, pairedHost] {
                                if (result.isSuccess()) {
                                    Settings::instance().add_host(pairedHost);
                                    MainTabs::getInstanse()->refillTabs();
                                    AddHostTab::startSearching();
                                } else {
                                    showError(result.error(), [] {
                                        AddHostTab::startSearching();
                                    });
                                }
                            });
                        });
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
