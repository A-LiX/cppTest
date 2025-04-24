#ifndef ANS_SOCKET_H
#define ANS_SOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

int ans_socket(int domain, int type, int protocol);
int ans_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int ans_close(int sockfd);

#ifdef __cplusplus
}
#endif

#endif // ANS_SOCKET_H 