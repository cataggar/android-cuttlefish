#pragma once

#include <sys/socket.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANDROID_SOCKET_ENV_PREFIX "ANDROID_SOCKET_"
#define ANDROID_SOCKET_DIR "/dev/socket"

#define SOCK_TYPE_MASK 0xf

int android_get_control_socket(const char* name);
int socket_local_client(const char* name, int namespaceId, int type);
int socket_local_client_connect(int fd, const char* name, int namespaceId,
                                int type);
int socket_local_server(const char* name, int namespaceId, int type);
int socket_local_server_bind(int s, const char* name, int namespaceId);
int socket_inaddr_any_server(int port, int type);
int socket_network_client(const char* host, int port, int type);
int socket_network_client_timeout(const char* host, int port, int type,
                                  int timeout, int* getaddrinfo_error);

#define ANDROID_SOCKET_NAMESPACE_ABSTRACT 0
#define ANDROID_SOCKET_NAMESPACE_RESERVED 1
#define ANDROID_SOCKET_NAMESPACE_FILESYSTEM 2

#ifdef __cplusplus
}
#endif
