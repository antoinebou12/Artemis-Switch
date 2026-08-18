/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
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

#include "http.h"
#include "CryptoManager.hpp"
#include "client.h"
#include "errors.h"
#include <borealis/core/logger.hpp>

#include <curl/curl.h>
#include <cstring>

static bool curlGlobalInit = false;
static std::string certificateFilePath;
static std::string keyFilePath;

CURL* makeCurl();
void freeCurl(CURL* curl);

struct HTTP_DATA {
    char* memory;
    size_t size;
    size_t limit;
    bool tooLarge;
};

static size_t _write_curl(void* contents, size_t size, size_t nmemb,
                          void* userp) {
    size_t realsize = size * nmemb;
    auto* mem = (HTTP_DATA*)userp;

    if (realsize > mem->limit || mem->size > mem->limit - realsize) {
        mem->tooLarge = true;
        return 0;
    }

    mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL)
        return 0;

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

int http_init(const std::string& key_directory) {
    if (!curlGlobalInit) {
#if LIBCURL_VERSION_NUM >= 0x075600
#ifdef USE_OPENSSL_CRYPTO
        curl_global_sslset(CURLSSLBACKEND_OPENSSL, NULL, NULL);
#elif USE_MBEDTLS_CRYPTO
        curl_global_sslset(CURLSSLBACKEND_MBEDTLS, NULL, NULL);
#endif
#endif
        curl_global_init(CURL_GLOBAL_ALL);
        brls::Logger::info("Curl: {}", curl_version());
    } else {
        return GS_OK;
    }

    certificateFilePath = key_directory + "/" + CERTIFICATE_FILE_NAME;
    keyFilePath = key_directory + "/" + KEY_FILE_NAME;

    curlGlobalInit = true;
    return GS_OK;
}

CURL* makeCurl() {
    auto curl = curl_easy_init();

    if (!curl)
        return nullptr;

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERT, certificateFilePath.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLKEY, keyFilePath.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_curl);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);

    return curl;
}

void freeCurl(CURL* curl) {
    curl_easy_cleanup(curl);
}

int http_request(const std::string& url, Data* data,
                 HTTPRequestTimeout timeout) {
    return http_request(url, data, timeout, {}, nullptr);
}

int http_request(const std::string& url, Data* data,
                 HTTPRequestTimeout timeout,
                 const HTTPRequestOptions& options,
                 HTTPResponseInfo* response) {
    if (!options.sensitive)
        brls::Logger::info("Curl: Request:\n{}", url.c_str());
    else
        brls::Logger::info("Curl: Sensitive authenticated request");

    auto* http_data = (HTTP_DATA*)malloc(sizeof(HTTP_DATA));
    http_data->memory = (char*)malloc(1);
    http_data->size = 0;
    http_data->limit = options.maxResponseBytes;
    http_data->tooLarge = false;

    auto curl = makeCurl();
    if (!curl) {
        free(http_data->memory);
        free(http_data);
        return GS_FAILED;
    }

    if (options.sensitive) curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, http_data);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    if (options.connectTimeoutMs > 0)
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                         options.connectTimeoutMs);
    if (options.totalTimeoutMs > 0)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, options.totalTimeoutMs);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    curl_slist* headers = nullptr;
    if (options.method == HTTPMethod::Post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, options.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(options.body.size()));
    }
    if (!options.contentType.empty()) {
        const std::string header = "Content-Type: " + options.contentType;
        headers = curl_slist_append(headers, header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (response) {
        response->status = status;
        response->responseTooLarge = http_data->tooLarge;
    }

    if (res != CURLE_OK) {
        const bool tooLarge = http_data->tooLarge;
        const char* message = tooLarge
            ? "HTTP response exceeded configured limit"
            : curl_easy_strerror(res);
        if (!options.suppressErrors)
            gs_set_error(message);
        brls::Logger::error("Curl: error: {}", message);
        free(http_data->memory);
        free(http_data);
        if (headers) curl_slist_free_all(headers);
        freeCurl(curl);
        return tooLarge ? GS_INVALID : GS_FAILED;
    } else if (http_data->memory == nullptr) {
        brls::Logger::error("Curl: memory = NULL");
        free(http_data->memory);
        free(http_data);
        if (headers) curl_slist_free_all(headers);
        freeCurl(curl);
        return GS_OUT_OF_MEMORY;
    }

    *data = Data(http_data->memory, http_data->size);

    if (options.sensitive) {
        brls::Logger::info("Curl: Sensitive response received ({} bytes, HTTP {})",
                           http_data->size, status);
    } else if (http_data->size > 3000) {
        brls::Logger::info("Curl: Response: Ok");
    } else {
        brls::Logger::info("Curl: Response:\n{}", http_data->memory);
    }

    free(http_data->memory);
    free(http_data);
    if (headers) curl_slist_free_all(headers);
    freeCurl(curl);

    if (status < 200 || status >= 300) {
        if (!options.suppressErrors)
            gs_set_error("HTTP request failed with status " +
                         std::to_string(status));
        return GS_ERROR;
    }
    return GS_OK;
}

void http_cleanup() {
    curl_global_cleanup();
}
