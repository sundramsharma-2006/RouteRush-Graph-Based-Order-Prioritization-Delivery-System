#pragma once
#include <cctype>
#include <map>
#include <sstream>
#include <string>

struct HttpRequest {
    std::string method;                                  
    std::string path;                                    
    std::string query;                                  
    std::map<std::string, std::string> headers;          
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
};

inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size() &&std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else out += s[i];
    }
    return out;
}

inline std::map<std::string, std::string> parseForm(const std::string& s) {
    std::map<std::string, std::string> m;
    std::size_t pos = 0;
    while (pos <= s.size()) {
        std::size_t amp = s.find('&', pos);
        std::string pair = s.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        if (!pair.empty()) {
            std::size_t eq = pair.find('=');
            std::string k = urlDecode(pair.substr(0, eq));
            std::string v = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
            if (!k.empty()) m[k] = v;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return m;
}

inline const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        default:  return "Internal Server Error";
    }
}

inline std::string serialize(const HttpResponse& r) {
    std::ostringstream o;
    o << "HTTP/1.1 " << r.status << ' ' << statusText(r.status) << "\r\n"
      << "Content-Type: " << r.contentType << "\r\n"
      << "Content-Length: " << r.body.size() << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Connection: close\r\n\r\n" << r.body;
    return o.str();
}

inline bool parseRequestHead(const std::string& head, HttpRequest& req) {
    std::istringstream in(head);
    std::string line;
    if (!std::getline(in, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream rl(line);
    std::string target, version;
    if (!(rl >> req.method >> target >> version)) return false;
    if (version.rfind("HTTP/", 0) != 0 || target.empty() || target[0] != '/') return false;
    std::size_t q = target.find('?');
    req.path = target.substr(0, q);
    req.query = q == std::string::npos ? "" : target.substr(q + 1);
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        std::size_t c = line.find(':');
        if (c == std::string::npos) return false;
        std::string k = line.substr(0, c), v = line.substr(c + 1);
        for (char& ch : k) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        std::size_t i = 0;
        while (i < v.size() && (v[i] == ' ' || v[i] == '\t')) ++i;
        req.headers[k] = v.substr(i);
    }
    return true;
}
