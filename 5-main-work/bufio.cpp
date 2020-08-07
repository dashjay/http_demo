#ifndef HTTPDEMO_BUFIO_HPP
#define HTTPDEMO_BUFIO_HPP

#include "sockpp/tcp_acceptor.h"
#include <string>


#define  MaxBufSize 4096
#define MaxConsecutiveEmptyReads 100


#define  ErrNegativeCount "negative count"
#define  ErrNoProgress "multiple Read calls return no data or error"
#define  ErrFillFullBuffer "tried to fill full buffer"
#define  ErrNegativeRead "reader returned negative count from Read"


namespace errors {
// http error class
    class Error : public std::exception {
    public:
        // error detail should be
        // 1. where is the error (in which file)
        // 2. what's the problem
        // 3. detail of it, may be a str or other thing
        std::string where, what, detail;

        Error(std::string &&_where,
              std::string &&_what,
              std::string _detail) : where{std::move(_where)},
                                     what{std::move(_what)},
                                     detail{std::move((_detail))} {}

        friend std::ostream &operator<<(std::ostream &out, Error &error) {
            out << error.where << ": " << error.what << ", detail: " << error.detail;
            return out;
        }

        std::string to_string() {
            return this->where + ":" + this->what + ", detail:" + this->detail;
        }
    };
} // namespace errors


namespace bufio {
    template<size_t siz>
    class BufReader {

    private:
        // use template for allocate the m_buf in stack for speed
        sockpp::tcp_socket *m_sock;
        char *m_r;
        char *m_w;
        int error_num;
        char m_buf[siz];

        void fill();

    public:

        explicit BufReader(sockpp::tcp_socket *sock) : m_sock{sock} {
            m_sock->read_timeout(std::chrono::seconds(30));
            reset();
        };


        void reset();

        std::string read_err();

        std::string peek(size_t n);

        size_t discard(size_t n);

        size_t buffered();

        char read_byte();

        std::pair<char *, char *> read_until(char delim);

        std::pair<char *, char *> readline();

        std::pair<char *, char *> read_n(size_t n);

        bool read_n(size_t n, std::string &buf);


        size_t write(const std::string &s) {
            return m_sock->write(s);
        }

        size_t write(char *buf, size_t n) {
            return m_sock->write(buf, n);
        }

        size_t write_n(const void *buf, size_t n) {
            return m_sock->write_n(buf, n);
        }
    };

    template<size_t siz>
    void BufReader<siz>::reset() {
        error_num = 0;
        m_r = m_w = m_buf;
    }

    template<size_t siz>
    void BufReader<siz>::fill() {
        // slide existing data to beginning

        if (m_r > m_buf) {
            int dist{static_cast<int>(m_w - m_r)};
            if (dist > 0) {
                std::memcpy(m_buf, m_r, dist);
                m_w -= dist;
                m_r = m_buf;
            } else {
                m_r = m_w = m_buf;
            }
        }

        errors::Error err("BufReader", "fill", ErrFillFullBuffer);


        if (static_cast<size_t>(m_w - m_buf) >= siz) {
            throw std::move(err);
        }

        for (auto i{MaxConsecutiveEmptyReads}; i > 0; i--) {
            auto n = m_sock->read(m_w, siz - (m_w - m_buf));
            if (n < 0) {
                err.detail = ErrNegativeCount;
                throw std::move(err);
            }
            m_w += n;
            if (m_sock->last_error() != 0) {
                error_num = m_sock->last_error();
                return;
            }
            if (n > 0) {
                return;
            }
        }
        err.detail = ErrNoProgress;
        throw std::move(err);
    }

    template<size_t siz>
    std::string BufReader<siz>::read_err() {
        if (error_num != 0) {
            return sockpp::tcp_socket::error_str(error_num);
        } else {
            return "";
        }
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::read_until(char delim) {
        errors::Error err("BufReader", "read_until", "");
        std::pair<char *, char *> res;
        for (;;) {
            // find from (m_r + s) -> m_w if there is a delim in them
            // s is a start indicator, we will not rescan on same mem
            // if we find delim in ....
            if (auto f{std::find(m_r, m_w, delim)}; *f == delim) {
                // ... make pair for return
                res = std::make_pair(m_r, f);

                // move m_r to next
                m_r = f + 1;
                return res;
            }
            // we can't find a delim
            if (error_num != 0) {
                err.detail = sockpp::tcp_socket::error_str(error_num);
                throw std::move(err);
            }

            if (buffered() >= siz) {
                err.detail = ErrFillFullBuffer;
                throw std::move(err);
            }

            fill(); // buffer is not full. fill it
        }
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::readline() {
        return this->read_until('\n');
    }

    template<size_t siz>
    size_t BufReader<siz>::buffered() {
        return m_w - m_r;
    }

    template<size_t siz>
    char BufReader<siz>::read_byte() {
        errors::Error err("BufReader", "read_byte", "");
        while (m_r == m_w) {
            if (error_num != 0) {
                err.detail = sockpp::tcp_socket::error_str(error_num);
                throw std::move(err);
            }
            fill();
        }
        char buf = *m_r;
        ++m_r;
        return buf;
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::read_n(size_t n) {
        auto max_try_time{10};
        while (static_cast<size_t>(m_w - m_r) < n) {
            fill();
            if (--max_try_time < 0) {
                return std::make_pair(nullptr, nullptr);
            }
        }
        char *p{m_r};
        m_r += n;
        return std::make_pair(p, p + n);
    }

    template<size_t siz>
    bool BufReader<siz>::read_n(size_t n, std::string &buf) {
        int remain{static_cast<int>(n)};
        int buf_siz{static_cast<int>(buffered())};
        if (remain < buf_siz) {
            buf.assign(m_r, remain);
            m_r += remain;
            return true;
        }
        buf.resize(n);
        memcpy(buf.data(), m_r, buf_siz);
        reset();
        remain -= buf_siz;
        m_sock->read_n(buf.data() + buf_siz, remain);
        return true;
    }

    template<size_t siz>
    std::string BufReader<siz>::peek(size_t n) {
        if (n < 0) {
            return std::string();
        }

        while (buffered() < n && buffered() < siz && error_num != 0) {
            fill();
        }
        std::string temp;

        if (auto avail = buffered();avail < n) {
            n = avail;
        }
        temp.clear();
        return temp.assign(m_r, m_r + n);
    }

}

#endif // HTTPDEMO_BUFIO_HPP