#pragma once
#include "Http.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace static_files {

inline bool safeUrlPath(const std::string& p) {
    if (p.empty() || p[0] != '/' || p.size() > 200) return false;
    if (p.find("..") != std::string::npos || p.find("//") != std::string::npos || p.find("/.") != std::string::npos) return false;
    for (unsigned char c : p)
        if (!(std::isalnum(c) || c == '/' || c == '.' || c == '-' || c == '_')) return false;
    return p.back() != '/';
}

// "" means: this extension is not served.
inline std::string mimeFor(const std::string& p) {
    std::size_t dot = p.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = p.substr(dot + 1);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (e == "html") return "text/html; charset=utf-8";
    if (e == "css")  return "text/css; charset=utf-8";
    if (e == "js")   return "application/javascript; charset=utf-8";
    if (e == "json") return "application/json; charset=utf-8";
    if (e == "svg")  return "image/svg+xml";
    if (e == "png")  return "image/png";
    if (e == "ico")  return "image/x-icon";
    if (e == "txt")  return "text/plain; charset=utf-8";
    return "";
}

inline HttpResponse notFound() {
    HttpResponse r; r.status = 404; r.contentType = "text/plain; charset=utf-8"; r.body = "not found"; return r;
}

// urlPath like "/css/style.css"; "/" maps to "/index.html".
inline HttpResponse serve(const std::string& root, std::string urlPath) {
    if (urlPath == "/") urlPath = "/index.html";
    if (!safeUrlPath(urlPath)) return notFound();
    std::string mime = mimeFor(urlPath);
    if (mime.empty()) return notFound();
    std::ifstream f(root + urlPath, std::ios::binary);
    if (!f) return notFound();
    std::ostringstream o;
    o << f.rdbuf();
    if (o.str().size() > 8u * 1024u * 1024u) return notFound();
    HttpResponse r;
    r.contentType = mime;
    r.body = o.str();
    return r;
}

}  // namespace static_files
