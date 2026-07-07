// gossip.cpp
#include "gossip.hpp"
#include "json.hpp"

#include <cstring>
#include <cerrno>
#include <iostream>

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

namespace {

// Best-effort line send to ip:port with a connect/send timeout. Returns success.
bool tcp_send(const std::string& ip, int port, const std::string& data, int timeout_s) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
        return false;

    int fd = -1;
    bool connected = false;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            connected = true;
        } else if (errno == EINPROGRESS) {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(fd, &wset);
            timeval tv{timeout_s, 0};
            if (::select(fd + 1, nullptr, &wset, nullptr, &tv) > 0) {
                int soerr = 0;
                socklen_t len = sizeof(soerr);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len);
                if (soerr == 0) connected = true;
            }
        }
        fcntl(fd, F_SETFL, flags);
        if (connected) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (!connected || fd < 0) {
        if (fd >= 0) ::close(fd);
        return false;
    }

    timeval tv{timeout_s, 0};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    std::string msg = data;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');

    size_t sent = 0;
    bool ok = true;
    while (sent < msg.size()) {
        ssize_t n = ::send(fd, msg.data() + sent, msg.size() - sent, 0);
        if (n <= 0) { ok = false; break; }
        sent += static_cast<size_t>(n);
    }
    ::close(fd);
    return ok;
}

}  // namespace

Gossip::Gossip(const Config& cfg, MemoryStore& mem) : cfg_(cfg), mem_(mem) {}

Gossip::~Gossip() { stop(); }

void Gossip::start() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) { std::cerr << "[gossip] socket() failed\n"; return; }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(cfg_.gossip_port));

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[gossip] bind() failed on port " << cfg_.gossip_port << "\n";
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    if (::listen(listen_fd_, 8) < 0) {
        std::cerr << "[gossip] listen() failed\n";
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    running_ = true;
    thread_ = std::thread([this] { listen_loop(); });
    std::cout << "[gossip] listening on port " << cfg_.gossip_port << "\n";
}

void Gossip::stop() {
    bool was = running_.exchange(false);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
    (void)was;
}

void Gossip::listen_loop() {
    while (running_) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(listen_fd_, &rset);
        timeval tv{1, 0};
        int rc = ::select(listen_fd_ + 1, &rset, nullptr, nullptr, &tv);
        if (rc <= 0) continue;
        if (!running_) break;

        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
        int cfd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &clen);
        if (cfd < 0) continue;

        timeval rtv{2, 0};
        setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));

        std::string buf;
        char chunk[2048];
        ssize_t n;
        while ((n = ::recv(cfd, chunk, sizeof(chunk), 0)) > 0) buf.append(chunk, static_cast<size_t>(n));
        ::close(cfd);

        size_t start = 0;
        while (start < buf.size()) {
            size_t nl = buf.find('\n', start);
            std::string line = (nl == std::string::npos) ? buf.substr(start) : buf.substr(start, nl - start);
            if (!line.empty()) handle_message(line);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }
}

void Gossip::handle_message(const std::string& line) {
    try {
        mj::Value m = mj::parse(line);
        const std::string type = m.find("type") ? m.find("type")->as_string() : "";

        if (type == "hb") {
            std::string id = m.find("id") ? m.find("id")->as_string() : "";
            double x = m.find("x") ? m.find("x")->as_number() : 0.0;
            double y = m.find("y") ? m.find("y")->as_number() : 0.0;
            if (!id.empty()) {
                std::lock_guard<std::mutex> lk(pmu_);
                peers_[id] = {x, y};
            }
        } else if (type == "mem") {
            std::string sender = m.find("sender") ? m.find("sender")->as_string() : "peer";
            MemoryItem it;
            it.id = m.find("id") ? m.find("id")->as_string() : "";
            it.content = m.find("content") ? m.find("content")->as_string() : "";
            it.trust_score = m.find("trust_score") ? m.find("trust_score")->as_number(0.5) : 0.5;
            it.hop_count = static_cast<int>(m.find("hop_count") ? m.find("hop_count")->as_number(0) : 0);
            if (!it.id.empty()) mem_.ingest_from_peer(sender, it);
        }
    } catch (const std::exception&) {
        // ignore malformed gossip
    }
}

std::map<std::string, std::pair<double, double>> Gossip::peer_positions() const {
    std::lock_guard<std::mutex> lk(pmu_);
    return peers_;
}

void Gossip::push(double self_x, double self_y, const MemoryStore& mem) {
    // heartbeat
    mj::Value hb = mj::Value::Object();
    hb.set("type", mj::Value::Str("hb"));
    hb.set("id", mj::Value::Str(cfg_.drone_id));
    hb.set("x", mj::Value::Num(self_x));
    hb.set("y", mj::Value::Num(self_y));
    const std::string hb_line = hb.dump();

    // one memory (newest)
    std::string mem_line;
    MemoryItem it;
    if (mem.newest_shareable(it)) {
        mj::Value mm = mj::Value::Object();
        mm.set("type", mj::Value::Str("mem"));
        mm.set("sender", mj::Value::Str(cfg_.drone_id));
        mm.set("id", mj::Value::Str(it.id));
        mm.set("content", mj::Value::Str(it.content));
        mm.set("trust_score", mj::Value::Num(it.trust_score));
        mm.set("hop_count", mj::Value::Num(it.hop_count));
        mem_line = mm.dump();
    }

    for (const auto& [pid, ip] : cfg_.peers) {
        (void)pid;
        tcp_send(ip, cfg_.gossip_port, hb_line, cfg_.gossip_timeout_s);
        if (!mem_line.empty()) tcp_send(ip, cfg_.gossip_port, mem_line, cfg_.gossip_timeout_s);
    }
}
