#include "headers.h"

Headers::Headers() {
    m_hdr.emplace("server", version);
}

bool Headers::has_header(const char *key) const {
    return m_hdr.find(key) != m_hdr.end();
}


size_t Headers::get_header_value_count(const char *key) const {
    auto r{m_hdr.equal_range(key)};
    return static_cast<size_t>(std::distance(r.first, r.second));
}

std::string Headers::get_header_value(const char *key, size_t id, char *def) const {
    auto rng = m_hdr.equal_range(key);
    auto it = rng.first;
    std::advance(it, static_cast<ssize_t>(id));
    if (it != rng.second) {
        return it->second;
    }
    return def;
}

void Headers::set_header(const char *key, const char *val) {
    if (this->has_header(key)) {
        m_hdr.erase(key);
    }
    m_hdr.emplace(key, val);
}

void Headers::set_header(const char *key, const std::string &val) {
    if (this->has_header(key)) {
        m_hdr.erase(key);
    }
    m_hdr.emplace(key, val);
}

void Headers::add_header(const char *key, const char *val) {
    m_hdr.emplace(key, val);
}

void Headers::add_header(const char *key, const std::string &val) {
    m_hdr.emplace(key, val);
}

void Headers::del_header(const char *key) {
    m_hdr.erase(key);
}

