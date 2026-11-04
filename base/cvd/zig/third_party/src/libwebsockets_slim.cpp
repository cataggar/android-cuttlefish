#include "libwebsockets.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct lws_vhost {
  int listen_fd = -1;
  int listen_port = -1;
  std::string iface;
  std::string mountpoint;
  std::string origin;
  std::string def_file;
};

struct lws_context {
  uint64_t options = 0;
  void* user = nullptr;
  std::vector<lws_vhost*> vhosts;
};

namespace {

int g_lws_log_level = LLL_ERR | LLL_WARN;
lws_log_emit_t g_lws_log_cb = nullptr;

std::string GuessMimeType(const std::string& path) {
  auto dot = path.rfind('.');
  if (dot == std::string::npos) return "application/octet-stream";
  std::string ext = path.substr(dot);
  if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
  if (ext == ".js" || ext == ".mjs") return "application/javascript; charset=utf-8";
  if (ext == ".css") return "text/css; charset=utf-8";
  if (ext == ".json") return "application/json; charset=utf-8";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".png") return "image/png";
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".ico") return "image/x-icon";
  if (ext == ".wasm") return "application/wasm";
  if (ext == ".txt") return "text/plain; charset=utf-8";
  return "application/octet-stream";
}

void WriteAll(int fd, const char* data, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t n = ::send(fd, data + off, len - off, MSG_NOSIGNAL);
    if (n <= 0) {
      if (errno == EINTR) continue;
      break;
    }
    off += static_cast<size_t>(n);
  }
}

void ServeHttpClient(int client_fd, const lws_vhost* vhost) {
  struct pollfd pfd;
  pfd.fd = client_fd;
  pfd.events = POLLIN;
  if (::poll(&pfd, 1, 200) <= 0) {
    ::close(client_fd);
    return;
  }

  char buf[4096];
  ssize_t n = ::recv(client_fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    ::close(client_fd);
    return;
  }
  buf[n] = '\0';

  std::istringstream iss(buf);
  std::string method, raw_target, version;
  iss >> method >> raw_target >> version;

  if (method != "GET" && method != "HEAD") {
    const char* resp =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    WriteAll(client_fd, resp, std::strlen(resp));
    ::close(client_fd);
    return;
  }

  // Strip query string and fragment
  auto qpos = raw_target.find_first_of("?#");
  std::string req_path =
      (qpos == std::string::npos) ? raw_target : raw_target.substr(0, qpos);
  if (req_path.empty() || req_path[0] != '/') {
    req_path = "/";
  }

  // Reject path traversal
  if (req_path.find("..") != std::string::npos) {
    const char* resp =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    WriteAll(client_fd, resp, std::strlen(resp));
    ::close(client_fd);
    return;
  }

  std::string rel_path = req_path;
  if (!vhost->mountpoint.empty() &&
      rel_path.compare(0, vhost->mountpoint.size(), vhost->mountpoint) == 0) {
    rel_path = rel_path.substr(vhost->mountpoint.size());
  }
  while (!rel_path.empty() && rel_path[0] == '/') {
    rel_path.erase(0, 1);
  }
  if (rel_path.empty()) {
    rel_path = vhost->def_file.empty() ? "index.html" : vhost->def_file;
  }

  std::string full_path = vhost->origin;
  if (!full_path.empty() && full_path.back() != '/') {
    full_path.push_back('/');
  }
  full_path += rel_path;

  struct stat st;
  if (::stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    if (!full_path.empty() && full_path.back() != '/') full_path.push_back('/');
    full_path += vhost->def_file.empty() ? "index.html" : vhost->def_file;
  }

  std::ifstream ifs(full_path, std::ios::binary);
  if (!ifs.is_open()) {
    const char* body = "404 Not Found\n";
    std::ostringstream hdr;
    hdr << "HTTP/1.1 404 Not Found\r\n"
        << "Content-Type: text/plain; charset=utf-8\r\n"
        << "Content-Length: " << std::strlen(body) << "\r\n"
        << "Connection: close\r\n\r\n";
    if (method != "HEAD") hdr << body;
    std::string resp = hdr.str();
    WriteAll(client_fd, resp.data(), resp.size());
    ::close(client_fd);
    return;
  }

  std::ostringstream body_ss;
  body_ss << ifs.rdbuf();
  std::string body = body_ss.str();

  std::ostringstream hdr;
  hdr << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: " << GuessMimeType(full_path) << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Cache-Control: no-cache\r\n"
      << "Connection: close\r\n\r\n";
  std::string header_str = hdr.str();
  WriteAll(client_fd, header_str.data(), header_str.size());
  if (method != "HEAD" && !body.empty()) {
    WriteAll(client_fd, body.data(), body.size());
  }
  ::close(client_fd);
}

lws_vhost* CreateVhostInternal(lws_context* context,
                               const struct lws_context_creation_info* info) {
  if (!context || !info) return nullptr;

  int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return nullptr;

  int opt = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  int port = info->port < 0 ? 0 : info->port;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (info->iface && info->iface[0] != '\0') {
    if (::inet_pton(AF_INET, info->iface, &addr.sin_addr) != 1) {
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
  } else {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  }

  if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return nullptr;
  }

  if (::listen(fd, 16) < 0) {
    ::close(fd);
    return nullptr;
  }

  struct sockaddr_in bound_addr;
  socklen_t bound_len = sizeof(bound_addr);
  std::memset(&bound_addr, 0, sizeof(bound_addr));
  if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&bound_addr),
                    &bound_len) == 0) {
    port = ntohs(bound_addr.sin_port);
  }

  auto* vh = new lws_vhost();
  vh->listen_fd = fd;
  vh->listen_port = port;
  if (info->iface) vh->iface = info->iface;
  if (info->mounts) {
    if (info->mounts->mountpoint) vh->mountpoint = info->mounts->mountpoint;
    if (info->mounts->origin) vh->origin = info->mounts->origin;
    if (info->mounts->def) vh->def_file = info->mounts->def;
  }
  context->vhosts.push_back(vh);
  return vh;
}

}  // namespace

extern "C" {

void lws_set_log_level(int level, lws_log_emit_t log_emit_function) {
  g_lws_log_level = level;
  g_lws_log_cb = log_emit_function;
}

void lwsl_emit_syslog(int level, const char* line) {
  (void)level;
  (void)line;
}

struct lws_context* lws_create_context(
    const struct lws_context_creation_info* info) {
  if (!info) return nullptr;
  auto* ctx = new lws_context();
  ctx->options = info->options;
  ctx->user = info->user;
  if ((info->options & LWS_SERVER_OPTION_EXPLICIT_VHOSTS) == 0) {
    CreateVhostInternal(ctx, info);
  }
  return ctx;
}

void lws_context_destroy(struct lws_context* context) {
  if (!context) return;
  for (lws_vhost* vh : context->vhosts) {
    if (vh->listen_fd >= 0) {
      ::close(vh->listen_fd);
    }
    delete vh;
  }
  delete context;
}

struct lws_vhost* lws_create_vhost(
    struct lws_context* context,
    const struct lws_context_creation_info* info) {
  return CreateVhostInternal(context, info);
}

void lws_vhost_destroy(struct lws_vhost* vh) {
  if (!vh) return;
  if (vh->listen_fd >= 0) {
    ::close(vh->listen_fd);
    vh->listen_fd = -1;
  }
}

int lws_get_vhost_listen_port(struct lws_vhost* vhost) {
  if (!vhost) return -1;
  return vhost->listen_port;
}

int lws_service(struct lws_context* context, int timeout_ms) {
  if (!context) return -1;
  if (context->vhosts.empty()) {
    if (timeout_ms > 0) {
      ::usleep(static_cast<useconds_t>(timeout_ms) * 1000);
    }
    return 0;
  }

  std::vector<struct pollfd> pfds;
  pfds.reserve(context->vhosts.size());
  for (lws_vhost* vh : context->vhosts) {
    if (vh->listen_fd >= 0) {
      struct pollfd pfd;
      pfd.fd = vh->listen_fd;
      pfd.events = POLLIN;
      pfd.revents = 0;
      pfds.push_back(pfd);
    }
  }

  int wait_ms = timeout_ms <= 0 ? 50 : timeout_ms;
  int rc = ::poll(pfds.data(), pfds.size(), wait_ms);
  if (rc <= 0) return 0;

  for (size_t i = 0; i < pfds.size(); ++i) {
    if (pfds[i].revents & POLLIN) {
      struct sockaddr_in cli_addr;
      socklen_t cli_len = sizeof(cli_addr);
      int client_fd = ::accept(
          pfds[i].fd, reinterpret_cast<struct sockaddr*>(&cli_addr), &cli_len);
      if (client_fd >= 0) {
        ServeHttpClient(client_fd, context->vhosts[i]);
      }
    }
  }
  return 0;
}

void lws_cancel_service(struct lws_context* context) { (void)context; }

int lws_callback_on_writable(struct lws* wsi) {
  (void)wsi;
  return 0;
}

int lws_write(struct lws* wsi, unsigned char* buf, size_t len,
              enum lws_write_protocol protocol) {
  (void)wsi;
  (void)buf;
  (void)protocol;
  return static_cast<int>(len);
}

struct lws_context* lws_get_context(const struct lws* wsi) {
  (void)wsi;
  return nullptr;
}

void* lws_wsi_user(struct lws* wsi) {
  (void)wsi;
  return nullptr;
}

}  // extern "C"
