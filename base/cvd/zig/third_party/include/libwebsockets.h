#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lws;
struct lws_context;
struct lws_vhost;
struct lws_protocol_vhost_options;

enum lws_callback_reasons {
  LWS_CALLBACK_HTTP = 12,
  LWS_CALLBACK_HTTP_BODY = 13,
  LWS_CALLBACK_HTTP_BODY_COMPLETION = 14,
  LWS_CALLBACK_HTTP_FILE_COMPLETION = 15,
  LWS_CALLBACK_HTTP_WRITEABLE = 16,
  LWS_CALLBACK_CLOSED_HTTP = 5,
};

typedef int lws_callback_function(struct lws* wsi,
                                  enum lws_callback_reasons reason, void* user,
                                  void* in, size_t len);

struct lws_protocols {
  const char* name;
  lws_callback_function* callback;
  size_t per_session_data_size;
  size_t rx_buffer_size;
  unsigned int id;
  void* user;
  size_t tx_packet_size;
};

#define LWS_PROTOCOL_LIST_TERM \
  { NULL, NULL, 0, 0, 0, NULL, 0 }

enum lws_mount_protocols {
  LWSMPRO_HTTP = 0,
  LWSMPRO_HTTPS = 1,
  LWSMPRO_FILE = 2,
  LWSMPRO_CGI = 3,
  LWSMPRO_REDIR_HTTP = 4,
  LWSMPRO_REDIR_HTTPS = 5,
  LWSMPRO_CALLBACK = 6,
};

struct lws_http_mount {
  const struct lws_http_mount* mount_next;
  const char* mountpoint;
  const char* origin;
  const char* def;
  const char* protocol;
  const struct lws_protocol_vhost_options* cgienv;
  const struct lws_protocol_vhost_options* extra_mimetypes;
  const struct lws_protocol_vhost_options* interpret;
  int cgi_timeout;
  int cache_max_age;
  unsigned int auth_mask;
  unsigned int cache_reusable : 1;
  unsigned int cache_revalidate : 1;
  unsigned int cache_intermediaries : 1;
  unsigned char origin_protocol;
  unsigned char mountpoint_len;
  const char* basic_auth_login_file;
  void* _unused[2];
};

#define LWS_SERVER_OPTION_EXPLICIT_VHOSTS (1ULL << 11)
#define LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT (1ULL << 12)

struct lws_context_creation_info {
  int port;
  const char* iface;
  const struct lws_protocols* protocols;
  const struct lws_extension* extensions;
  const struct lws_token_limits* token_limits;
  const char* ssl_private_key_password;
  const char* ssl_cert_filepath;
  const char* ssl_private_key_filepath;
  const char* ssl_ca_filepath;
  const char* ssl_cipher_list;
  const char* http_proxy_address;
  unsigned int http_proxy_port;
  int gid;
  int uid;
  uint64_t options;
  void* user;
  int ka_time;
  int ka_probes;
  int ka_interval;
  const struct lws_http_mount* mounts;
  const char* server_string;
  const char* vhost_name;
};

#define LLL_ERR (1 << 0)
#define LLL_WARN (1 << 1)
#define LLL_NOTICE (1 << 2)
#define LLL_INFO (1 << 3)
#define LLL_DEBUG (1 << 4)
#define LLL_PARSER (1 << 5)
#define LLL_HEADER (1 << 6)
#define LLL_EXT (1 << 7)
#define LLL_CLIENT (1 << 8)
#define LLL_LATENCY (1 << 9)
#define LLL_USER (1 << 10)
#define LLL_THREAD (1 << 11)

typedef void (*lws_log_emit_t)(int level, const char* line);

enum lws_write_protocol {
  LWS_WRITE_TEXT = 0,
  LWS_WRITE_BINARY = 1,
  LWS_WRITE_CONTINUATION = 2,
  LWS_WRITE_HTTP = 3,
};

void lws_set_log_level(int level, lws_log_emit_t log_emit_function);

int lws_callback_http_dummy(struct lws* wsi, enum lws_callback_reasons reason,
                            void* user, void* in, size_t len);

struct lws_context* lws_create_context(
    const struct lws_context_creation_info* info);
void lws_context_destroy(struct lws_context* context);

struct lws_vhost* lws_create_vhost(
    struct lws_context* context, const struct lws_context_creation_info* info);
void lws_vhost_destroy(struct lws_vhost* vh);
int lws_get_vhost_listen_port(struct lws_vhost* vhost);

int lws_service(struct lws_context* context, int timeout_ms);
void lws_cancel_service(struct lws_context* context);

#ifdef __cplusplus
}
#endif
