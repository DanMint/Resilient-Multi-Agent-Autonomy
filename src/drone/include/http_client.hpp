// http_client.hpp -- a minimal, dependency-free HTTP/1.1 client.
//
// Only what the body needs: a single POST to a localhost service, with connect
// and read timeouts so a slow or dead brain can never hang the control loop.
// Throws std::runtime_error on connection failure; callers convert that into a
// safe fallback rather than crashing.
#pragma once

#include <string>

struct HttpResponse {
    int status = 0;
    std::string body;
};

namespace http {

// POST `body` to http://host:port/path. Throws std::runtime_error on any
// network-level failure (DNS, connect, timeout, closed connection).
HttpResponse post(const std::string& host, int port, const std::string& path,
                  const std::string& body, const std::string& content_type,
                  int timeout_seconds);

// Exposed for unit testing: parse a raw HTTP response buffer into status + body.
HttpResponse parse_response(const std::string& raw);

}  // namespace http
