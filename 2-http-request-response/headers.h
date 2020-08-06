#ifndef HTTP_TEST_HEADERS_H
#define HTTP_TEST_HEADERS_H

#include <map>
#include <string>

const std::string version = "http-demo-1";

class Headers {
    using Hdr = std::multimap<std::string, std::string>;
private:
    Hdr m_hdr;
public:
    Headers();

    bool has_header(const char *key) const;

    size_t get_header_value_count(const char *key) const;

    std::string get_header_value(const char *key, size_t id = 0, char *def = nullptr) const;

    void set_header(const char *key, const char *val);

    void set_header(const char *key, const std::string &val);

    void add_header(const char *key, const char *val);

    void add_header(const char *key, const std::string &val);

    void del_header(const char *key);

    const Hdr &hdr();
};


#endif //HTTP_TEST_HEADERS_H
