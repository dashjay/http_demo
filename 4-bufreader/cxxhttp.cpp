#include "cxxhttp.h"
#include "spdlog/spdlog.h"
#include <sstream>

Request::Request(
        std::string &&m,
        std::string &&u,
        std::string &&b) : method{m},
                           uri{u},
                           body{b},
                           proto{"HTTP/1.1"} {

}

Request::Request(
        std::string &m,
        std::string &u,
        std::string &b) : method{m},
                          uri{u},
                          body{b},
                          proto{"HTTP/1.1"} {

}

std::string Request::to_string() {
    std::ostringstream out;
    out << this->method << ' ' << this->uri << ' ' << this->proto << "\r\n";
    for (auto &kv:this->headers.m_hdr) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    out << "\r\n";
    if (!this->body.empty()) {
        out << this->body;
    }
    return out.str();
}

std::string Response::to_string() {
    std::ostringstream out;
    out << this->proto << ' ' << this->status << "\r\n";
    for (auto &kv:this->headers.m_hdr) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    out << "\r\n";
    if (!this->body.empty()) {
        out << this->body;
    }
    return out.str();
}


namespace parser {

    bool parse_request(bufio::BufReader<MaxBufSize> &buf, Request &req) {

        auto req_slice{buf.readline()};
        if (!parse_request_line(req_slice.first, req_slice.second, req)) {
            return false;
        }
        if (!parse_hdr(buf, req.headers)) {
            return false;
        }
        if (req.ContentLength() > 0) {
            auto body_slice{buf.read_n(req.ContentLength())};
            if (body_slice.first == nullptr || body_slice.second == nullptr) {
                return false;
            }
            req.body.assign(body_slice.first, body_slice.second);
        }
        return true;
    }

    bool parse_response(bufio::BufReader<MaxBufSize> &buf, Response &resp) {
        std::pair<char *, char *> resp_slice;
        resp_slice = buf.readline();
        if (!parse_response_line(resp_slice.first, resp_slice.second, resp)) {
            return false;
        }

        // parse headers
        if (!parse_hdr(buf, resp.headers)) {
            return false;
        }

        // if we get head...
        if (resp.req != nullptr && resp.req->method == "HEAD") {
            // ... we can ignore body (not exsits)
            return true;
        }

        return parse_response_body(buf, resp);
    }

    bool parse_response_line(const char *beg, const char *end, Response &resp) {
        if (end - beg <= 2) {
            spdlog::error("no space, end-beg = {}", end - beg);
            return false;
        }

        auto p_end{end};

        auto space{std::find(beg, end, ' ')};

        if (*space != ' ') {
            spdlog::error("no space, space = {}", *space);
            return false;
        }
        resp.proto.assign(beg, space);
        while (*p_end == '\r' || *p_end == '\n') {
            p_end--;
        }
        resp.status.assign(space + 1, p_end + 1);
        space = std::find(space + 1, p_end, ' ');
        resp.status_code = static_cast<int>(std::strtol(space + 1, nullptr, 10));
        return true;
    }

    bool parse_hdr(bufio::BufReader<MaxBufSize> &buf, Headers &hdr) {
        for (;;) {
            auto hdr_slice{buf.readline()};

            if (*hdr_slice.first == '\r' && *hdr_slice.second == '\n') {
                break; // \r\n means header read over
            }
            if (!parse_single_header(hdr_slice.first, hdr_slice.second, hdr)) {
                std::cerr
                        << "parse_single_header error, detail:" + std::string(hdr_slice.first, hdr_slice.second)
                        << '\n';
            }
        }
        return true;
    }

    bool parse_request_line(const char *beg, const char *end, Request &req) {
        auto p_end{end};
        auto space1{std::find(beg, end, ' ')};
        if (*space1 != ' ') {
            return false;
        }
        req.method.assign(beg, space1);
        auto space2{std::find(space1 + 1, end, ' ')};
        if (*space2 != ' ') {
            return false;
        }
        req.uri.assign(space1 + 1, space2);
        while (*p_end == '\r' || *p_end == '\n') {
            p_end--;
        }
        req.proto.assign(space2 + 1, p_end + 1);
        return true;
    }

    bool parse_single_header(const char *beg, const char *end, Headers &hdr) {
        auto p = beg;
        while (p < end && *p != ':') {
            p++;
        }
        if (p < end) {
            auto key_end = p;
            p++; // skip ':'
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++;
            }
            if (p < end) {
                auto val_begin = p;
                while (p < end && *p != '\r' && *p != '\n') {
                    p++;
                }
                hdr.m_hdr.emplace(std::string(beg, key_end), std::string(val_begin, end - 1));
                return true;
            }
        }
        return false;
    }

    bool parse_response_body(bufio::BufReader<MaxBufSize> &buf, Response &resp) {
        if (resp.ContentLength() > 0) {
            auto body{buf.read_n(resp.ContentLength())};
            resp.body.assign(body.first, body.second);
        } else {
            if (resp.headers.has_header("Transfer-Encoding") &&
                resp.headers.get_header_value("Transfer-Encoding") == "chunked") {
                for (;;) {
                    auto res = buf.readline();
                    if (res.first == nullptr || res.second == nullptr) {
                        return false;
                    }
                    auto chunk_size = static_cast<int>(std::strtol(res.first, nullptr, 16));
                    if (chunk_size > 0) {
                        buf.read_n(chunk_size, resp.body);
                        resp.headers.set_header("Content-Length", std::to_string(resp.body.size()));
                    } else if (chunk_size == 0) {
                        break;
                    } else {
                        assert("chunk_size < 0" && false);
                    }
                }
                resp.headers.del_header("Transfer-Encoding");
            }
        }
        return true;
    }
}