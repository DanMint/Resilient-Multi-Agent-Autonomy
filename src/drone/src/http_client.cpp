// http_client.cpp -- minimal HTTP/1.1 POST over a TCP socket.
#include "http_client.hpp"

#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

namespace http {

namespace {

// Connect to host:port with a timeout. Returns a connected fd, or throws.
int connect_with_timeout(const std::string& host, int port, int timeout_seconds) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        throw std::runtime_error("http: cannot resolve " + host);

    int fd = -1;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        // non-blocking connect so we can bound it with select()
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);  // restore blocking
            break;
        }
        if (errno == EINPROGRESS) {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(fd, &wset);
            timeval tv{timeout_seconds, 0};
            rc = ::select(fd + 1, nullptr, &wset, nullptr, &tv);
            if (rc > 0) {
                int soerr = 0;
                socklen_t len = sizeof(soerr);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len);
                if (soerr == 0) { fcntl(fd, F_SETFL, flags); break; }  // connected
            }
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) throw std::runtime_error("http: cannot connect to " + host + ":" + std::to_string(port));

    // apply send/recv timeouts
    timeval tv{timeout_seconds, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return fd;
}

void send_all(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) throw std::runtime_error("http: send failed");
        sent += static_cast<size_t>(n);
    }
}

// case-insensitive search for a header value
long content_length_of(const std::string& headers) {
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string key = "content-length:";
    size_t p = lower.find(key);
    if (p == std::string::npos) return -1;
    p += key.size();
    while (p < headers.size() && (headers[p] == ' ' || headers[p] == '\t')) ++p;
    long val = 0;
    bool any = false;
    while (p < headers.size() && headers[p] >= '0' && headers[p] <= '9') { val = val * 10 + (headers[p] - '0'); ++p; any = true; }
    return any ? val : -1;
}

}  // namespace

HttpResponse parse_response(const std::string& raw) {
    HttpResponse r;
    size_t header_end = raw.find("\r\n\r\n");
    std::string headers = (header_end == std::string::npos) ? raw : raw.substr(0, header_end);
    r.body = (header_end == std::string::npos) ? "" : raw.substr(header_end + 4);

    // status line: "HTTP/1.1 200 OK"
    size_t sp = headers.find(' ');
    if (sp != std::string::npos) {
        r.status = std::atoi(headers.c_str() + sp + 1);
    }
    return r;
}

HttpResponse post(const std::string& host, int port, const std::string& path,
                  const std::string& body, const std::string& content_type,
                  int timeout_seconds) {
    int fd = connect_with_timeout(host, port, timeout_seconds);

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << host << ":" << port << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;

    try {
        send_all(fd, req.str());

        // read headers first
        std::string buf;
        char chunk[4096];
        size_t header_end = std::string::npos;
        while ((header_end = buf.find("\r\n\r\n")) == std::string::npos) {
            ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buf.append(chunk, static_cast<size_t>(n));
        }
        if (header_end == std::string::npos) { ::close(fd); throw std::runtime_error("http: no response headers"); }

        long clen = content_length_of(buf.substr(0, header_end));
        size_t have_body = buf.size() - (header_end + 4);

        if (clen >= 0) {
            while (have_body < static_cast<size_t>(clen)) {
                ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
                if (n <= 0) break;
                buf.append(chunk, static_cast<size_t>(n));
                have_body += static_cast<size_t>(n);
            }
        } else {
            // no Content-Length: read until the server closes (Connection: close)
            ssize_t n;
            while ((n = ::recv(fd, chunk, sizeof(chunk), 0)) > 0) buf.append(chunk, static_cast<size_t>(n));
        }

        ::close(fd);
        return parse_response(buf);
    } catch (...) {
        ::close(fd);
        throw;
    }
}

}  // namespace http
