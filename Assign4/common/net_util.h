#ifndef NET_UTIL_H
#define NET_UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
using namespace std;


inline void setNoDelay(int fd) {
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
}

// Buffered line reader over a blocking TCP socket: one instance per connection.
class LineSocket {
public:
    explicit LineSocket(int fd) : fd_(fd) {}

    // Returns false on EOF/error.
    bool readLine(string& out) {
        out.clear();
        while (true) {
            size_t nl = buf_.find('\n');
            if (nl != string::npos) {
                out = buf_.substr(0, nl);
                buf_.erase(0, nl + 1);
                return true;
            }
            char tmp[4096];
            ssize_t n = recv(fd_, tmp, sizeof(tmp), 0);
            if (n <= 0) return false;
            buf_.append(tmp, n);
        }
    }

    bool writeLine(const string& line) {
        string msg = line + "\n";
        size_t sent = 0;
        while (sent < msg.size()) {
            ssize_t n = send(fd_, msg.data() + sent, msg.size() - sent, 0);
            if (n <= 0) return false;
            sent += n;
        }
        return true;
    }

    int fd() const { return fd_; }

private:
    int fd_;
    string buf_;
};

#endif
