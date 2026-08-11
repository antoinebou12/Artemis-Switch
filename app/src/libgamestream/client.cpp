/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "Settings.hpp"
#include "client.h"
#include "CryptoManager.hpp"
#include "errors.h"
#include "http.h"
#include "../features/host/HostAddressParse.hpp"
#include "../features/stream/FrameRateOptions.hpp"
#include <Limelight.h>
#include <borealis/core/logger.hpp>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sstream>

#define CHANNEL_COUNT_STEREO 2
#define CHANNEL_COUNT_51_SURROUND 6

#define CHANNEL_MASK_STEREO 0x3
#define CHANNEL_MASK_51_SURROUND 0xFC

static std::string unique_id = "0123456789ABCDEF";

int extractVersionQuadFromString(const char* string, int* quad) {
    const char* nextNumber = string;
    for (int i = 0; i < 4; i++) {
        // Parse the next component
        quad[i] = (int)strtol(nextNumber, (char**)&nextNumber, 10);

        // Skip the dot if we still have version components left.
        //
        // We continue looping even when we're at the end of the
        // input string to ensure all subsequent version components
        // are zeroed.
        if (*nextNumber != 0) {
            nextNumber++;
        }
    }

    return 0;
}

bool _SERVER_DATA::isSunshine() const {
    int AppVersionQuad[4];
    extractVersionQuadFromString(serverInfoAppVersion.c_str(), AppVersionQuad);
    return AppVersionQuad[3] < 0;
}

bool _SERVER_DATA::isApollo() const {
    return virtualDisplayCapable || virtualDisplayDriverReady ||
           hasApolloPermissionField || !serverCommands.empty() ||
           !currentGameUuid.empty();
}

static int load_serverinfo(PSERVER_DATA server, bool https) {
    int ret = GS_INVALID;
    char url[4096];
    std::string pairedText;
    std::string currentGameText;
    std::string stateText;
    std::string httpsPortText;
    std::string optionalText;

    // Modern GFE versions don't allow serverinfo to be fetched over HTTPS
    // if the client is not already paired. Since we can't pair without
    // knowing the server version, we make another request over HTTP if the
    // HTTPS request fails. We can't just use HTTP for everything because it
    // doesn't accurately tell us if we're paired.

    snprintf(url, sizeof(url), "%s://%s:%d/serverinfo?uniqueid=%s",
             https ? "https" : "http", server->serverInfo.address,
             https ? server->httpsPort : server->httpPort, unique_id.c_str());

    Data data;

    if (http_request(url, &data, HTTPRequestTimeoutLow) != GS_OK) {
        ret = GS_IO_ERROR;
        goto cleanup;
    }

    if (xml_status(data) == GS_ERROR) {
        ret = GS_ERROR;
        goto cleanup;
    }

    if (xml_search(data, "currentgame", &currentGameText) != GS_OK) {
        goto cleanup;
    }

    if (xml_search(data, "PairStatus", &pairedText) != GS_OK)
        goto cleanup;

    if (xml_search(data, "appversion", &server->serverInfoAppVersion) !=
        GS_OK) {
        goto cleanup;
    }

    if (xml_search(data, "state", &stateText) != GS_OK)
        goto cleanup;

    if (xml_search(data, "ServerCodecModeSupport",
                   &server->serverInfo.serverCodecModeSupport) != GS_OK)
        goto cleanup;

    if (xml_search(data, "gputype", &server->gpuType) != GS_OK)
        goto cleanup;

    if (xml_search(data, "GsVersion", &server->gsVersion) != GS_OK)
        goto cleanup;

    if (xml_search(data, "hostname", &server->hostname) != GS_OK)
        goto cleanup;

    if (xml_search(data, "GfeVersion", &server->serverInfoGfeVersion) != GS_OK)
        goto cleanup;

    if (xml_search(data, "HttpsPort", &httpsPortText) != GS_OK)
        goto cleanup;

    if (xml_search(data, "mac", &server->mac) != GS_OK)
        goto cleanup;

    // Apollo extensions are optional. Their absence is the authoritative
    // Sunshine fallback and must never make standard server-info fail.
    xml_search(data, "currentgameuuid", &server->currentGameUuid);
    if (xml_search(data, "VirtualDisplayCapable", &optionalText) == GS_OK)
        server->virtualDisplayCapable = optionalText == "true" || optionalText == "1";
    optionalText.clear();
    if (xml_search(data, "VirtualDisplayDriverReady", &optionalText) == GS_OK)
        server->virtualDisplayDriverReady = optionalText == "true" || optionalText == "1";
    optionalText.clear();
    if (xml_search(data, "Permission", &optionalText) == GS_OK && !optionalText.empty()) {
        try {
            server->permission = static_cast<uint32_t>(std::stoul(optionalText, nullptr, 0));
            server->hasApolloPermissionField = true;
        } catch (...) {
            server->permission = 0;
            server->hasApolloPermissionField = false;
        }
    }
    xml_search_all(data, "ServerCommand", &server->serverCommands);

    // These fields are present on all version of GFE that this client
    // supports
    if (currentGameText.empty() || pairedText.empty() ||
        server->serverInfoAppVersion.empty() || stateText.empty()) {
        goto cleanup;
    }

    server->paired = pairedText == "1";
    server->currentGame =
        currentGameText.empty() ? 0 : atoi(currentGameText.c_str());
    server->supports4K = server->serverInfo.serverCodecModeSupport != 0;
    server->serverMajorVersion = atoi(server->serverInfoAppVersion.c_str());
    server->httpsPort = atoi(httpsPortText.c_str());
    if (!server->httpsPort)
        server->httpsPort = 47984;

    if (stateText == "_SERVER_BUSY") {
        // After GFE 2.8, current game remains set even after streaming
        // has ended. We emulate the old behavior by forcing it to zero
        // if streaming is not active.
        server->currentGame = 0;
    }
    ret = GS_OK;

cleanup:
    return ret;
}

static int load_server_status(PSERVER_DATA server) {
    int ret = GS_INVALID;
    int i;

    /* Fetch the HTTPS port if we don't have one yet */
    if (!server->httpsPort) {
        ret = load_serverinfo(server, false);
        if (ret != GS_OK)
            return ret;
    }

    // Modern GFE versions don't allow serverinfo to be fetched over HTTPS if the client
    // is not already paired. Since we can't pair without knowing the server version, we
    // make another request over HTTP if the HTTPS request fails. We can't just use HTTP
    // for everything because it doesn't accurately tell us if we're paired.
    ret = GS_INVALID;
    for (i = 0; i < 2 && ret != GS_OK; i++) {
        ret = load_serverinfo(server, i == 0);
    }

    if (ret == GS_OK) {
        if (server->serverMajorVersion > MAX_SUPPORTED_GFE_VERSION) {
            gs_set_error(
                "Ensure you're running the latest version of "
                "Moonlight-Switch or downgrade GeForce Experience and try again");
            ret = GS_UNSUPPORTED_VERSION;
        } else if (server->serverMajorVersion < MIN_SUPPORTED_GFE_VERSION) {
            gs_set_error(
                "Moonlight-Switch requires a newer version of GeForce "
                "Experience. Please upgrade GFE on your PC and try again.");
            ret = GS_UNSUPPORTED_VERSION;
        }
    }

    return ret;
}

static std::string _gs_error = "";

void gs_set_error(std::string error) { _gs_error = error; }

std::string gs_error() {
    if (_gs_error.empty()) {
        return "Unknown error...";
    }
    return _gs_error;
}

int gs_unpair(PSERVER_DATA server) {
    int ret = GS_OK;
    char url[4096];

    Data data;

    snprintf(url, sizeof(url), "http://%s:%u/unpair?uniqueid=%s",
             server->serverInfo.address,
             server->httpPort,
             unique_id.c_str());
    ret = http_request(url, &data, HTTPRequestTimeoutLow);
    return ret;
}

static int gs_pair_validate(Data& data, std::string* result) {
    *result = "";

    int ret = GS_OK;
    if ((ret = xml_status(data) != GS_OK)) {
        return ret;
    } else if ((ret = xml_search(data, "paired", result)) != GS_OK) {
        return ret;
    }

    //    if (strcmp(*result, "1") != 0) {
    //        gs_error = "Pairing failed";
    //        ret = GS_FAILED;
    //    }

    return ret;
}

static int gs_pair_cleanup(int ret, PSERVER_DATA server, std::string* result) {
    if (ret != GS_OK) {
        gs_unpair(server);
    }
    return ret;
}

int gs_pair(PSERVER_DATA server, char* pin) {
    int ret = GS_OK;
    Data data;
    std::string result;
    char url[4096];

    if (server->paired) {
        gs_set_error("Already paired");
        return GS_WRONG_STATE;
    }

    if (server->currentGame != 0) {
        gs_set_error(
            "The computer is currently in a game. You must close the game "
            "before pairing");
        return GS_WRONG_STATE;
    }

    brls::Logger::info("Client: Pairing with generation {} server",
                       server->serverMajorVersion);
    brls::Logger::info("Client: Start pairing stage #1");

    Data salt = Data::random_bytes(16);
    Data salted_pin = salt.append(Data(pin, strlen(pin)));
//    brls::Logger::info("Client: PIN: {}, salt {}", pin, salt.hex().bytes());

    snprintf(url, sizeof(url),
             "http://%s:%u/"
             "pair?uniqueid=%s&devicename=roth&updateState=1&phrase="
             "getservercert&salt=%s&clientcert=%s",
             server->serverInfo.address, 
             server->httpPort,
             unique_id.c_str(), salt.hex().bytes(),
             CryptoManager::cert_data().hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = xml_search(data, "plaincert", &result)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #2");

    Data plainCert = Data((char*)result.c_str(), result.size());
    Data aesKey;

    // Gen 7 servers use SHA256 to get the key
    int hashLength;
    if (server->serverMajorVersion >= 7) {
        aesKey = CryptoManager::create_AES_key_from_salt_SHA256(salted_pin);
        hashLength = 32;
    } else {
        aesKey = CryptoManager::create_AES_key_from_salt_SHA1(salted_pin);
        hashLength = 20;
    }

    Data randomChallenge = Data::random_bytes(16);
    Data encryptedChallenge =
        CryptoManager::aes_encrypt(randomChallenge, aesKey);

    snprintf(
        url, sizeof(url),
        "http://%s:%u/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&clientchallenge=%s",
        server->serverInfo.address, 
        server->httpPort,
        unique_id.c_str(),
        encryptedChallenge.hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if (xml_search(data, "challengeresponse", &result) != GS_OK) {
        ret = GS_INVALID;
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #3");

    Data encServerChallengeResp =
        Data((char*)result.c_str(), result.size()).hex_to_bytes();
    Data decServerChallengeResp =
        CryptoManager::aes_decrypt(encServerChallengeResp, aesKey);
    Data serverResponse = decServerChallengeResp.subdata(0, hashLength);
    Data serverChallenge = decServerChallengeResp.subdata(hashLength, 16);

    Data clientSecret = Data::random_bytes(16);
    Data challengeRespHashInput =
        serverChallenge
            .append(CryptoManager::signature(CryptoManager::cert_data()))
            .append(clientSecret);

    Data challengeRespHash;

    if (server->serverMajorVersion >= 7) {
        challengeRespHash =
            CryptoManager::SHA256_hash_data(challengeRespHashInput);
    } else {
        challengeRespHash =
            CryptoManager::SHA1_hash_data(challengeRespHashInput);
    }
    Data challengeRespEncrypted =
        CryptoManager::aes_encrypt(challengeRespHash, aesKey);

    snprintf(
        url, sizeof(url),
        "http://%s:%u/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&serverchallengeresp=%s",
        server->serverInfo.address,
        server->httpPort,
        unique_id.c_str(),
        challengeRespEncrypted.hex().bytes());

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if (xml_search(data, "pairingsecret", &result) != GS_OK) {
        ret = GS_INVALID;
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #4");

    Data serverSecretResp =
        Data((char*)result.c_str(), result.size()).hex_to_bytes();
    Data serverSecret = serverSecretResp.subdata(0, 16);
    Data serverSignature = serverSecretResp.subdata(16, 256);

    if (!CryptoManager::verify_signature(serverSecret, serverSignature,
                                         plainCert.hex_to_bytes())) {
        gs_set_error("MITM attack detected");
        ret = GS_FAILED;
        return gs_pair_cleanup(ret, server, &result);
    }

    Data serverChallengeRespHashInput =
        randomChallenge
            .append(CryptoManager::signature(plainCert.hex_to_bytes()))
            .append(serverSecret);
    Data serverChallengeRespHash;

    if (server->serverMajorVersion >= 7) {
        serverChallengeRespHash =
            CryptoManager::SHA256_hash_data(serverChallengeRespHashInput);
    } else {
        serverChallengeRespHash =
            CryptoManager::SHA1_hash_data(serverChallengeRespHashInput);
    }

    Data clientPairingSecret = clientSecret.append(
        CryptoManager::sign_data(clientSecret, CryptoManager::key_data()));

    snprintf(
        url, sizeof(url),
        "http://%s:%u/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&clientpairingsecret=%s",
        server->serverInfo.address, 
        server->httpPort,
        unique_id.c_str(),
        clientPairingSecret.hex().bytes());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    brls::Logger::info("Client: Start pairing stage #5");

    snprintf(
        url, sizeof(url),
        "https://%s:%u/"
        "pair?uniqueid=%s&devicename=roth&updateState=1&phrase=pairchallenge",
        server->serverInfo.address, server->httpsPort, unique_id.c_str());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) != GS_OK) {
        return gs_pair_cleanup(ret, server, &result);
    }

    if ((ret = gs_pair_validate(data, &result) != GS_OK)) {
        return gs_pair_cleanup(ret, server, &result);
    }

    server->paired = true;

    return gs_pair_cleanup(ret, server, &result);
}

int gs_applist(PSERVER_DATA server, PAPP_LIST* list) {
    int ret = GS_OK;
    char url[4096];
    Data data;

    snprintf(url, sizeof(url), "https://%s:%u/applist?uniqueid=%s",
             server->serverInfo.address, server->httpsPort, unique_id.c_str());

    if (http_request(url, &data, HTTPRequestTimeoutMedium) != GS_OK)
        ret = GS_IO_ERROR;
    else if (xml_status(data) == GS_ERROR)
        ret = GS_ERROR;
    else if (xml_applist(data, list) != GS_OK)
        ret = GS_INVALID;
    return ret;
}

int gs_app_boxart(PSERVER_DATA server, int app_id, Data* out) {
    int ret = GS_OK;
    char url[4096];
    Data data;

    snprintf(
        url, sizeof(url),
        "https://%s:%u/appasset?uniqueid=%s&appid=%d&AssetType=2&AssetIdx=0",
        server->serverInfo.address, server->httpsPort, unique_id.c_str(), app_id);

    if (http_request(url, &data, HTTPRequestTimeoutMedium) != GS_OK) {
        ret = GS_IO_ERROR;
    } else {
        *out = data;
    }
    return ret;
}

int gs_start_app(PSERVER_DATA server, STREAM_CONFIGURATION* config, int appId,
                 bool sops, bool localaudio, int gamepad_mask,
                 const APOLLO_LAUNCH_OPTIONS* apolloOptions) {
    int ret = GS_OK;
    std::string result;

    if (config->height >= 2160 && !server->supports4K) {
        gs_set_error("4K not supported");
        return GS_NOT_SUPPORTED_4K;
    }

    Data rand = Data::random_bytes(16);
    memcpy(config->remoteInputAesKey, rand.bytes(), 16);

    char url[4096];
    int rikeyid = 0;

    Data data;

    std::string apolloQuery;
    if (apolloOptions && server->isApollo()) {
        if (apolloOptions->virtualDisplay) {
            if (!server->virtualDisplayCapable) {
                gs_set_error("Apollo virtual display is not supported by this host");
                return GS_NOT_SUPPORTED_4K;
            }
            if (!server->virtualDisplayDriverReady) {
                gs_set_error("Apollo virtual display driver is not ready; install or enable the driver on the host");
                return GS_WRONG_STATE;
            }
            apolloQuery += "&virtualDisplay=1";
        }
        if (!apolloOptions->appUuid.empty())
            apolloQuery += "&appuuid=" + apolloOptions->appUuid;
        if (apolloOptions->width > 0 && apolloOptions->height > 0)
            apolloQuery += "&resolution=" + std::to_string(apolloOptions->width) +
                           "x" + std::to_string(apolloOptions->height);
        if (apolloOptions->refreshRate > 0)
            apolloQuery += "&refreshRate=" + std::to_string(apolloOptions->refreshRate);
        int scaleFactor = apolloOptions->scaleFactor;
        if (scaleFactor <= 0)
            scaleFactor = 100;
        apolloQuery += "&scaleFactor=" + std::to_string(scaleFactor);
    }

    if (server->currentGame == 0) {
        int channelCounnt =
            config->audioConfiguration == AUDIO_CONFIGURATION_STEREO
                ? CHANNEL_COUNT_STEREO
                : CHANNEL_COUNT_51_SURROUND;
        int mask = config->audioConfiguration == AUDIO_CONFIGURATION_STEREO
                       ? CHANNEL_MASK_STEREO
                       : CHANNEL_MASK_51_SURROUND;
        const bool apolloHost = server->isApollo();
        int fps = artemis::stream::launchModeFpsValue(
            config->fps, config->clientRefreshRateX100, apolloHost);
        // GFE SOPS path: keep integer fps <= 60. Apollo millihertz modes skip this.
        if (!apolloHost && sops && config->fps > 60)
            fps = 60;
        snprintf(url, sizeof(url),
                 "https://%s:%u/"
                 "launch?uniqueid=%s&appid=%d&mode=%dx%dx%d&additionalStates=1&"
                 "sops=%d&rikey=%s&rikeyid=%d&localAudioPlayMode=%d&"
                 "surroundAudioInfo=%d&remoteControllersBitmap=%d&gcmap=%d%s",
                 server->serverInfo.address, server->httpsPort, unique_id.c_str(), appId,
                 config->width, config->height, fps, sops, rand.hex().bytes(),
                 rikeyid, localaudio, (mask << 16) + channelCounnt,
                 gamepad_mask, gamepad_mask,
                 (std::string(LiGetLaunchUrlQueryParameters()) + apolloQuery).c_str());
    } else {
        snprintf(url, sizeof(url),
                 "https://%s:%u/resume?uniqueid=%s&rikey=%s&rikeyid=%d%s",
                 server->serverInfo.address, server->httpsPort, unique_id.c_str(),
                 rand.hex().bytes(), rikeyid,
                 (std::string(LiGetLaunchUrlQueryParameters()) + apolloQuery).c_str());
    }

    if ((ret = http_request(url, &data, HTTPRequestTimeoutLong)) == GS_OK) {
        server->currentGame = appId;
    } else {
        goto exit;
    }

    if ((ret = xml_status(data) != GS_OK)) {
        goto exit;
    } else if ((ret = xml_search(data, "gamesession", &result)) != GS_OK) {
        goto exit;
    }

    if (result == "0") {
        ret = GS_FAILED;
        goto exit;
    }

    if (xml_search(data, "sessionUrl0", &result) == GS_OK) {
        const std::string::size_type size = result.size();
        server->serverInfo.rtspSessionUrl = new char[size + 1];
        memcpy((void *) server->serverInfo.rtspSessionUrl, result.c_str(), size + 1);
    } else {
        brls::Logger::error("sessionUrl0 not found");
    }

exit:
    return ret;
}

int gs_quit_app(PSERVER_DATA server) {
    int ret = GS_OK;
    char url[4096];
    std::string result;
    Data data;

    snprintf(url, sizeof(url), "https://%s:%u/cancel?uniqueid=%s",
             server->serverInfo.address, server->httpsPort, unique_id.c_str());
    if ((ret = http_request(url, &data, HTTPRequestTimeoutMedium)) != GS_OK)
        goto exit;

    if ((ret = xml_status(data) != GS_OK)) {
        goto exit;
    } else if ((ret = xml_search(data, "cancel", &result)) != GS_OK) {
        goto exit;
    }

    if (result == "0") {
        ret = GS_FAILED;
        goto exit;
    }

exit:
    return ret;
}

namespace {
constexpr uint32_t ApolloPermissionClipboardSet = 0x00010000;
constexpr uint32_t ApolloPermissionClipboardRead = 0x00020000;
constexpr size_t ApolloClipboardLimit = 64 * 1024;
}

int gs_clipboard_get(PSERVER_DATA server, std::string* text, long* httpStatus) {
    if (!server || !text || !server->isApollo()) {
        gs_set_error("Clipboard is unsupported by this host");
        return GS_NOT_SUPPORTED_4K;
    }
    if (server->hasApolloPermissionField &&
        (server->permission & ApolloPermissionClipboardRead) == 0) {
        gs_set_error("Apollo denied clipboard read permission");
        return GS_WRONG_STATE;
    }
    char url[4096];
    snprintf(url, sizeof(url), "https://%s:%u/actions/clipboard?type=text",
             server->serverInfo.address, server->httpsPort);
    Data data;
    HTTPRequestOptions options;
    options.maxResponseBytes = ApolloClipboardLimit;
    options.sensitive = true;
    HTTPResponseInfo response;
    const int result = http_request(url, &data, HTTPRequestTimeoutMedium,
                                    options, &response);
    if (httpStatus) *httpStatus = response.status;
    if (result != GS_OK) return result;
    *text = std::string(reinterpret_cast<const char*>(data.bytes()), data.size());
    return GS_OK;
}

int gs_clipboard_set(PSERVER_DATA server, const std::string& text,
                     long* httpStatus) {
    if (!server || !server->isApollo()) {
        gs_set_error("Clipboard is unsupported by this host");
        return GS_NOT_SUPPORTED_4K;
    }
    if (text.size() > ApolloClipboardLimit) {
        gs_set_error("Clipboard text exceeds the 64 KiB limit");
        return GS_INVALID;
    }
    if (server->hasApolloPermissionField &&
        (server->permission & ApolloPermissionClipboardSet) == 0) {
        gs_set_error("Apollo denied clipboard upload permission");
        return GS_WRONG_STATE;
    }
    char url[4096];
    snprintf(url, sizeof(url), "https://%s:%u/actions/clipboard?type=text",
             server->serverInfo.address, server->httpsPort);
    Data data;
    HTTPRequestOptions options;
    options.method = HTTPMethod::Post;
    options.body = text;
    options.contentType = "text/plain; charset=utf-8";
    options.maxResponseBytes = 4096;
    options.sensitive = true;
    HTTPResponseInfo response;
    const int result = http_request(url, &data, HTTPRequestTimeoutMedium,
                                    options, &response);
    if (httpStatus) *httpStatus = response.status;
    return result;
}

int gs_init(PSERVER_DATA server, const std::string address) {
    const auto parsed = artemis::host::parse_host_address(address);
    unsigned short httpPort = 47989; // Default HTTP port
    if (parsed.port.has_value()) {
        httpPort = *parsed.port;
    }

    if (parsed.host.empty()) {
        gs_set_error("Address is Empty");
        return GS_FAILED;
    }
    
    if (!CryptoManager::load_cert_key_pair()) {
        brls::Logger::info("Client: No certs, generate new...");

        if (!CryptoManager::generate_new_cert_key_pair()) {
            brls::Logger::info("Client: Failed to generate certs...");
            return GS_FAILED;
        }
    }

    http_init(Settings::instance().key_dir());

    LiInitializeServerInformation(&server->serverInfo);
    server->address = parsed.host;
    server->serverInfo.address = server->address.c_str();
    server->httpPort = httpPort;
    server->httpsPort = 0; /* Populated by load_server_status() */

    int result = load_server_status(server);
    server->serverInfo.serverInfoAppVersion =
        server->serverInfoAppVersion.c_str();
    server->serverInfo.serverInfoGfeVersion =
        server->serverInfoGfeVersion.c_str();
    return result;
}
