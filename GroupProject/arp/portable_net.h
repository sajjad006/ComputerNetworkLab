#ifndef PORTABLE_NET_H
#define PORTABLE_NET_H

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define CLOSESOCK closesocket
  #define SOCK_ERR SOCKET_ERROR
  inline int net_init() {
      WSADATA wsa;
      return WSAStartup(MAKEWORD(2, 2), &wsa);
  }
  inline void net_cleanup() { WSACleanup(); }
  inline int set_reuse(SOCKET s) {
      int opt = 1;
      return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  }
  inline void sleep_ms(int ms) { Sleep(ms); }
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
  typedef int SOCKET;
  #define INVALID_SOCKET (-1)
  #define CLOSESOCK close
  #define SOCK_ERR (-1)
  inline int net_init() { return 0; }
  inline void net_cleanup() {}
  inline int set_reuse(SOCKET s) {
      int opt = 1;
      return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  }
  inline void sleep_ms(int ms) {
      usleep((useconds_t)ms * 1000);
  }
#endif

// TCP is a byte stream: one recv() can return half a packet, or two packets glued
// together. This reads exactly `len` bytes. Returns len, or <= 0 if the peer left.
inline int recvAll(SOCKET s, char* buf, int len) {
    int got = 0;
    while (got < len) {
        int n = recv(s, buf + got, len - got, 0);
        if (n <= 0) return n;
        got += n;
    }
    return got;
}

#endif
