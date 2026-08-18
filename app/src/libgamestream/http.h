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

#pragma once

#include "Data.hpp"

enum HTTPRequestTimeout : long {
    HTTPRequestTimeoutLow = 1,
    HTTPRequestTimeoutMedium = 5,
    HTTPRequestTimeoutLong = 120
};

enum class HTTPMethod { Get, Post };

struct HTTPRequestOptions {
    HTTPMethod method = HTTPMethod::Get;
    std::string body;
    std::string contentType;
    size_t maxResponseBytes = 4 * 1024 * 1024;
    bool sensitive = false;
    bool suppressErrors = false;
    long connectTimeoutMs = 0;
    long totalTimeoutMs = 0;
};

struct HTTPResponseInfo {
    long status = 0;
    bool responseTooLarge = false;
};

int http_init(const std::string& key_directory);
int http_request(const std::string& url, Data* data, HTTPRequestTimeout timeout);
int http_request(const std::string& url, Data* data, HTTPRequestTimeout timeout,
                 const HTTPRequestOptions& options, HTTPResponseInfo* response);
