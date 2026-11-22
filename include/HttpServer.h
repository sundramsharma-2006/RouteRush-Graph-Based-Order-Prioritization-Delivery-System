// ===== MEMBER 1 (Sundram Sharma) : tiny HTTP server on a TCP socket =====
// One request per connection ("Connection: close"). Each connection is read in its own thread (browsers open
// several at once, some of them idle), but the request handler itself runs under one mutex, so the shared
// application state is never touched by two requests at the same time.
// Listens on 127.0.0.1 ONLY: reachable from this computer, not from the network.
// Build with -pthread; Windows also needs -lws2_32.
#pragma once
#include "Http.h"
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using sock_t = SOCKET;
  static const sock_t kBadSock = INVALID_SOCKET;
  inline void closeSock(sock_t s) { closesocket(s); }
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <unistd.h>
  using sock_t = int;
  static const sock_t kBadSock = -1;
  inline void closeSock(sock_t s) { close(s); }
#endif

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    static constexpr std::size_t kMaxHead = 64 * 1024, kMaxBody = 1024 * 1024;

    ~HttpServer() { if (listen_ != kBadSock) closeSock(listen_); }

    // Returns "" on success or an error text (e.g. the port is already in use).
    std::string start(int port) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "WSAStartup failed";
#endif
        listen_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_ == kBadSock) return "could not create socket";
#ifndef _WIN32
        int yes = 1;
        setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
#endif
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<unsigned short>(port));
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listen_, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0)
            return "port " + std::to_string(port) + " is not available (is RouteRush already running?)";
        if (::listen(listen_, 8) != 0) return "listen failed";
        return "";
    }

    // Serves until the process is stopped (Ctrl+C).
    void serve(Handler handler) {
        while (true) {
            sock_t c = ::accept(listen_, nullptr, nullptr);
            if (c == kBadSock) continue;
            std::thread([this, c, handler] { handleConnection(c, handler); }).detach();
        }
    }

private:
    void handleConnection(sock_t c, const Handler& handler) {
        setTimeout(c, 5);
        HttpResponse resp;
        HttpRequest req;
        int code = readRequest(c, req);
        if (code != 0) { resp.status = code; resp.body = "{\"error\":\"bad request\"}"; }
        else {
            std::lock_guard<std::mutex> lock(handlerMutex_);
            try { resp = handler(req); }
            catch (...) { resp.status = 500; resp.body = "{\"error\":\"internal error\"}"; }
        }
        std::string out = serialize(resp);
        std::size_t sent = 0;
        while (sent < out.size()) {
            int n = ::send(c, out.data() + sent, static_cast<int>(out.size() - sent), 0);
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
        closeSock(c);
    }

    static void setTimeout(sock_t s, int seconds) {
#ifdef _WIN32
        DWORD ms = static_cast<DWORD>(seconds * 1000);
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof ms);
#else
        timeval tv{}; tv.tv_sec = seconds;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
    }

    // 0 = ok, otherwise the HTTP status to answer with.
    static int readRequest(sock_t c, HttpRequest& req) {
        std::string buf;
        char tmp[4096];
        std::size_t headEnd = std::string::npos;
        while (headEnd == std::string::npos) {
            int n = ::recv(c, tmp, sizeof tmp, 0);
            if (n <= 0) return 400;
            buf.append(tmp, static_cast<std::size_t>(n));
            headEnd = buf.find("\r\n\r\n");
            if (headEnd == std::string::npos && buf.size() > kMaxHead) return 413;
        }
        if (!parseRequestHead(buf.substr(0, headEnd), req)) return 400;
        std::size_t want = 0;
        auto it = req.headers.find("content-length");
        if (it != req.headers.end()) {
            try { want = static_cast<std::size_t>(std::stoul(it->second)); } catch (...) { return 400; }
            if (want > kMaxBody) return 413;
        }
        req.body = buf.substr(headEnd + 4);
        while (req.body.size() < want) {
            int n = ::recv(c, tmp, sizeof tmp, 0);
            if (n <= 0) return 400;
            req.body.append(tmp, static_cast<std::size_t>(n));
        }
        req.body.resize(want);
        return 0;
    }

    sock_t listen_ = kBadSock;
    std::mutex handlerMutex_;
};
