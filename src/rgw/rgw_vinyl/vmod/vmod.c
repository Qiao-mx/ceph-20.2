/*-
 * RGW VinylCache VMOD for Vinyl Cache
 *
 * This VMOD provides integration between Vinyl Cache (vinyld) and RGW,
 * allowing VCL to call RGW for request processing.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include "vdef.h"
#include "vas.h"
#include "vrt.h"
#include "vcc_rgw_vinyl_if.h"

#include "cache/cache.h"
#include "vcl.h"

/* Magic number for VMOD private data */
#define VMOD_RGW_VINYL_PRIV_MAGIC 0xDEADBEEF
/* Magic number for request private data */
#define VMOD_RGW_VINYL_REQ_MAGIC 0xCAFEBABE

/* Default paths */
#define DEFAULT_CONFIG_PATH "/etc/rgw/vinyl.conf"
#define DEFAULT_IPC_SOCKET_PATH "/var/run/rgw_vinyl.sock"
#define MAX_HEADER_SIZE 8192
#define MAX_BODY_SIZE (64 * 1024 * 1024)  /* 64MB */

/* VMOD private data structure */
struct vmod_rgw_vinyl_priv {
    unsigned magic;
#define VMOD_RGW_VINYL_PRIV_MAGIC 0xDEADBEEF
    char *config_path;
    char *ipc_socket_path;
    int initialized;
    int socket_fd;
};

/* Request private data structure */
struct vmod_rgw_vinyl_req {
    unsigned magic;
#define VMOD_RGW_VINYL_REQ_MAGIC 0xCAFEBABE
    struct vmod_rgw_vinyl_priv *priv;
    char *method;
    char *uri;
    char *host;
    int cache_hit;
    char *response_headers;
    char *response_body;
    int response_status;
};

/* Global initialization flag */
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int vmod_initialized = 0;

/* Forward declarations */
static struct vmod_rgw_vinyl_req *vmod_req_new(VRT_CTX,
    struct vmod_rgw_vinyl_priv *priv);
static void vmod_req_free(VRT_CTX, struct vmod_rgw_vinyl_req *req);

/* IPC communication functions */
static int ipc_connect(const char *socket_path) {
    int fd;
    struct sockaddr_un addr;

    if (socket_path == NULL)
        return -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void ipc_disconnect(int fd) {
    if (fd >= 0)
        close(fd);
}

static ssize_t ipc_send_request(int fd, const char *method, const char *uri,
                                const char *host, const char *headers,
                                const char *body, size_t body_len) {
    struct iovec iov[6];
    struct {
        uint32_t magic;
        uint32_t body_len;
        uint32_t header_len;
        uint32_t uri_len;
        uint32_t method_len;
        uint32_t host_len;
    } hdr;
    ssize_t total = 0;
    ssize_t n;

    /* Prepare header */
    hdr.magic = 0x52415631;  /* "RAV1" */
    hdr.body_len = body_len;
    hdr.header_len = headers ? strlen(headers) : 0;
    hdr.uri_len = uri ? strlen(uri) : 0;
    hdr.method_len = method ? strlen(method) : 0;
    hdr.host_len = host ? strlen(host) : 0;

    iov[0].iov_base = &hdr;
    iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = (void *)method;
    iov[1].iov_len = hdr.method_len;
    iov[2].iov_base = (void *)uri;
    iov[2].iov_len = hdr.uri_len;
    iov[3].iov_base = (void *)host;
    iov[3].iov_len = hdr.host_len;
    iov[4].iov_base = (void *)headers;
    iov[4].iov_len = hdr.header_len;
    iov[5].iov_base = (void *)body;
    iov[5].iov_len = body_len;

    n = writev(fd, iov, 6);
    if (n > 0)
        total += n;

    return total;
}

static ssize_t ipc_recv_response(int fd, int *status, char **headers,
                                 char **body, size_t *body_len) {
    struct {
        uint32_t magic;
        uint32_t status;
        uint32_t header_len;
        uint32_t body_len;
    } resp_hdr;
    ssize_t n;

    /* Read response header */
    n = read(fd, &resp_hdr, sizeof(resp_hdr));
    if (n != sizeof(resp_hdr))
        return -1;

    if (resp_hdr.magic != 0x52415632)  /* "RAV2" */
        return -1;

    *status = resp_hdr.status;

    /* Read headers */
    if (resp_hdr.header_len > 0 && resp_hdr.header_len < MAX_HEADER_SIZE) {
        *headers = malloc(resp_hdr.header_len + 1);
        if (*headers == NULL)
            return -1;
        n = read(fd, *headers, resp_hdr.header_len);
        if (n != (ssize_t)resp_hdr.header_len) {
            free(*headers);
            *headers = NULL;
            return -1;
        }
        (*headers)[resp_hdr.header_len] = '\0';
    } else {
        *headers = NULL;
    }

    /* Read body */
    if (resp_hdr.body_len > 0 && resp_hdr.body_len < MAX_BODY_SIZE) {
        *body = malloc(resp_hdr.body_len + 1);
        if (*body == NULL) {
            if (*headers)
                free(*headers);
            *headers = NULL;
            return -1;
        }
        n = read(fd, *body, resp_hdr.body_len);
        if (n != (ssize_t)resp_hdr.body_len) {
            free(*body);
            free(*headers);
            *body = NULL;
            *headers = NULL;
            return -1;
        }
        (*body)[resp_hdr.body_len] = '\0';
        *body_len = resp_hdr.body_len;
    } else {
        *body = NULL;
        *body_len = 0;
    }

    return 0;
}

/* Event function - called on VCL_EVENT_LOAD, USE, DISCARD, etc. */
int vmod_event(VRT_CTX, enum vcl_event_e e) {
    struct vmod_rgw_vinyl_priv *priv;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(ctx->vcl);
    AN(ctx->vcl->name);

    switch (e) {
    case VCL_EVENT_LOAD:
        /* VMOD loading - allocate private data */
        AZ(ctx->specific);
        ALLOC_OBJ(priv, VMOD_RGW_VINYL_PRIV_MAGIC);
        AN(priv);
        priv->config_path = NULL;
        priv->ipc_socket_path = NULL;
        priv->initialized = 0;
        priv->socket_fd = -1;
        ctx->specific = priv;
        WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache VMOD loaded");
        break;

    case VCL_EVENT_USE:
        /* VMOD being used - ready for requests */
        if (ctx->specific != NULL) {
            CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);
            WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache VMOD in use");
        }
        break;

    case VCL_EVENT_DISCARD:
        /* VMOD unloading - cleanup */
        if (ctx->specific != NULL) {
            CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);
            if (priv->socket_fd >= 0)
                ipc_disconnect(priv->socket_fd);
            if (priv->config_path)
                free(priv->config_path);
            if (priv->ipc_socket_path)
                free(priv->ipc_socket_path);
            FREE_OBJ(priv);
        }
        pthread_mutex_lock(&init_mutex);
        vmod_initialized = 0;
        pthread_mutex_unlock(&init_mutex);
        WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache VMOD discarded");
        break;

    case VCL_EVENT_WARM:
        /* VCL being warmed up */
        break;

    case VCL_EVENT_COLD:
        /* VCL being cooled down */
        break;

    default:
        break;
    }

    return 0;
}

/* Initialize the VMOD */
VCL_VOID v_matchproto_(td_rgw_vinyl_init)
vmod_init(VRT_CTX, VCL_STRING config_path, VCL_STRING ipc_socket_path) {
    struct vmod_rgw_vinyl_priv *priv;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(ctx->vcl);

    if (ctx->specific == NULL) {
        VRT_fail(ctx, "rgw_vinyl.init(): VMOD not properly initialized");
        return;
    }
    CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);

    /* Free any existing paths */
    if (priv->config_path) {
        free(priv->config_path);
        priv->config_path = NULL;
    }
    if (priv->ipc_socket_path) {
        free(priv->ipc_socket_path);
        priv->ipc_socket_path = NULL;
    }

    /* Set config path */
    if (config_path != NULL && *config_path != '\0')
        priv->config_path = strdup(config_path);
    else
        priv->config_path = strdup(DEFAULT_CONFIG_PATH);

    /* Set IPC socket path */
    if (ipc_socket_path != NULL && *ipc_socket_path != '\0')
        priv->ipc_socket_path = strdup(ipc_socket_path);
    else
        priv->ipc_socket_path = strdup(DEFAULT_IPC_SOCKET_PATH);

    /* Connect to IPC socket */
    if (priv->socket_fd >= 0)
        ipc_disconnect(priv->socket_fd);

    priv->socket_fd = ipc_connect(priv->ipc_socket_path);
    if (priv->socket_fd < 0) {
        WSL(ctx->vsl, SLT_Error,
            "rgw_vinyl.init(): Failed to connect to IPC socket %s: %s",
            priv->ipc_socket_path, strerror(errno));
        /* Don't fail - allow init to succeed for warm VCL */
    }

    priv->initialized = 1;

    pthread_mutex_lock(&init_mutex);
    vmod_initialized = 1;
    pthread_mutex_unlock(&init_mutex);

    WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache VMOD initialized");
    if (ctx->vsl)
        WSLb(ctx->vsl, SLT_VCL_Log, "  config: %s",
             priv->config_path ? priv->config_path : "(default)");
    if (ctx->vsl)
        WSLb(ctx->vsl, SLT_VCL_Log, "  IPC socket: %s",
             priv->ipc_socket_path ? priv->ipc_socket_path : "(default)");
}

/* Cleanup function */
VCL_VOID v_matchproto_(td_rgw_vinyl_fini)
vmod_fini(VRT_CTX) {
    struct vmod_rgw_vinyl_priv *priv;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

    if (ctx->specific == NULL)
        return;

    CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);

    if (priv->socket_fd >= 0) {
        ipc_disconnect(priv->socket_fd);
        priv->socket_fd = -1;
    }

    priv->initialized = 0;

    pthread_mutex_lock(&init_mutex);
    vmod_initialized = 0;
    pthread_mutex_unlock(&init_mutex);

    WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache VMOD finalized");
}

/* Get VMOD version */
VCL_STRING v_matchproto_(td_rgw_vinyl_version)
vmod_version(VRT_CTX) {
    (void)ctx;
    return "1.0.0";
}

/* Ping RGW backend */
VCL_INT v_matchproto_(td_rgw_vinyl_ping)
vmod_ping(VRT_CTX) {
    struct vmod_rgw_vinyl_priv *priv;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

    if (ctx->specific == NULL)
        return 0;

    CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);

    if (priv->socket_fd < 0) {
        /* Try to reconnect */
        priv->socket_fd = ipc_connect(priv->ipc_socket_path);
        if (priv->socket_fd < 0)
            return 0;
    }

    return 1;
}

/* Cache lookup */
VCL_INT v_matchproto_(td_rgw_vinyl_cache_lookup)
vmod_cache_lookup(VRT_CTX, VCL_STRING key) {
    (void)ctx;
    (void)key;
    /* TODO: Implement cache lookup via IPC or shared memory */
    return 0;
}

/* Cache invalidate */
VCL_INT v_matchproto_(td_rgw_vinyl_cache_invalidate)
vmod_cache_invalidate(VRT_CTX, VCL_STRING key) {
    (void)ctx;
    (void)key;
    /* TODO: Implement cache invalidation via IPC */
    return 0;
}

/* Cleanup function */
VCL_VOID v_matchproto_(td_rgw_vinyl_cleanup)
vmod_cleanup(VRT_CTX) {
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    WSL(ctx->vsl, SLT_VCL_Log, "RGW VinylCache cleanup called");
}

/* Request object - init */
int vmod_req_init(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr,
                  struct vmod_rgw_vinyl_priv *priv) {
    struct vmod_rgw_vinyl_req *req;

    AN(req_ptr);

    ALLOC_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);
    AN(req);

    req->priv = priv;
    req->method = NULL;
    req->uri = NULL;
    req->host = NULL;
    req->cache_hit = 0;
    req->response_headers = NULL;
    req->response_body = NULL;
    req->response_status = 0;

    *req_ptr = req;
    return 0;
}

/* Request object - fini */
void vmod_req_fini(VRT_CTX, struct vmod_rgw_vinyl_req *req) {
    CHECK_OBJ_NOTNULL(req, VMOD_RGW_VINYL_REQ_MAGIC);

    if (req->method)
        free(req->method);
    if (req->uri)
        free(req->uri);
    if (req->host)
        free(req->host);
    if (req->response_headers)
        free(req->response_headers);
    if (req->response_body)
        free(req->response_body);

    FREE_OBJ(req);
}

/* Request object start */
VCL_VOID v_matchproto_(td_rgw_vinyl_request_start)
vmod_req_start(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr,
               VCL_STRING method, VCL_STRING uri, VCL_STRING host) {
    struct vmod_rgw_vinyl_req *req;
    struct vmod_rgw_vinyl_priv *priv;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(req_ptr);

    if (*req_ptr == NULL) {
        /* First call - need to get priv from ctx */
        if (ctx->specific == NULL) {
            VRT_fail(ctx, "rgw_vinyl.request.start(): VMOD not initialized");
            return;
        }
        CAST_OBJ(priv, ctx->specific, VMOD_RGW_VINYL_PRIV_MAGIC);
        vmod_req_init(ctx, req_ptr, priv);
    }

    req = *req_ptr;
    CHECK_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);

    /* Store request data */
    if (req->method)
        free(req->method);
    if (req->uri)
        free(req->uri);
    if (req->host)
        free(req->host);

    req->method = method ? strdup(method) : NULL;
    req->uri = uri ? strdup(uri) : NULL;
    req->host = host ? strdup(host) : NULL;
    req->cache_hit = 0;

    WSL(ctx->vsl, SLT_VCL_Log, "rgw_vinyl.request.start: %s %s",
        req->method ? req->method : "???",
        req->uri ? req->uri : "???");
}

/* Request object send */
VCL_INT v_matchproto_(td_rgw_vinyl_request_send)
vmod_req_send(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr) {
    struct vmod_rgw_vinyl_req *req;
    struct vmod_rgw_vinyl_priv *priv;
    int fd;
    ssize_t n;
    const char *h;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(req_ptr);

    if (*req_ptr == NULL) {
        WSL(ctx->vsl, SLT_Error,
            "rgw_vinyl.request.send(): Request not started");
        return 0;
    }

    req = *req_ptr;
    CHECK_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);
    priv = req->priv;

    if (!req->method || !req->uri) {
        WSL(ctx->vsl, SLT_Error,
            "rgw_vinyl.request.send(): Method or URI not set");
        return 0;
    }

    /* Get socket - may need to reconnect */
    fd = priv->socket_fd;
    if (fd < 0) {
        fd = ipc_connect(priv->ipc_socket_path);
        if (fd < 0) {
            WSL(ctx->vsl, SLT_Error,
                "rgw_vinyl.request.send(): Cannot connect to RGW");
            return 0;
        }
        priv->socket_fd = fd;
    }

    /* Build headers from request using VRT interface */
    char headers[MAX_HEADER_SIZE];
    int header_len = 0;

    /* Add Host header */
    if (req->host) {
        header_len += snprintf(headers + header_len,
                              MAX_HEADER_SIZE - header_len,
                              "Host: %s\r\n", req->host);
    }

    /* Get common headers using http_GetReqHeader */
    struct http *hp = ctx->req->http;
#define RGW_VINYL_HEADER(hdr, name) do { \
        h = http_GetReqHeader(hp, HDR_REQ, hdr); \
        if (h) { \
            header_len += snprintf(headers + header_len, \
                                  MAX_HEADER_SIZE - header_len, \
                                  "%s: %s\r\n", name, h); \
        } \
    } while (0)

    RGW_VINYL_HEADER("Accept:", "Accept");
    RGW_VINYL_HEADER("Accept-Encoding:", "Accept-Encoding");
    RGW_VINYL_HEADER("Accept-Language:", "Accept-Language");
    RGW_VINYL_HEADER("Authorization:", "Authorization");
    RGW_VINYL_HEADER("Cache-Control:", "Cache-Control");
    RGW_VINYL_HEADER("Content-Type:", "Content-Type");
    RGW_VINYL_HEADER("Content-Length:", "Content-Length");
    RGW_VINYL_HEADER("If-Modified-Since:", "If-Modified-Since");
    RGW_VINYL_HEADER("If-None-Match:", "If-None-Match");
    RGW_VINYL_HEADER("Range:", "Range");

#undef RGW_VINYL_HEADER

    /* Send request via IPC */
    n = ipc_send_request(fd, req->method, req->uri, req->host,
                         headers, NULL, 0);
    if (n < 0) {
        WSL(ctx->vsl, SLT_Error,
            "rgw_vinyl.request.send(): IPC send failed");
        ipc_disconnect(fd);
        priv->socket_fd = -1;
        return 0;
    }

    /* Receive response via IPC */
    n = ipc_recv_response(fd, &req->response_status,
                          &req->response_headers, &req->response_body, NULL);
    if (n < 0) {
        WSL(ctx->vsl, SLT_Error,
            "rgw_vinyl.request.send(): IPC recv failed");
        ipc_disconnect(fd);
        priv->socket_fd = -1;
        return 0;
    }

    WSL(ctx->vsl, SLT_VCL_Log,
        "rgw_vinyl.request.send(): Got response status %d",
        req->response_status);

    return 1;
}

/* Request object get_header */
VCL_STRING v_matchproto_(td_rgw_vinyl_request_get_header)
vmod_req_get_header(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr,
                    VCL_STRING name) {
    struct vmod_rgw_vinyl_req *req;
    static char value[MAX_HEADER_SIZE];
    const char *p;
    int len;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(req_ptr);
    AN(name);

    if (*req_ptr == NULL)
        return NULL;

    req = *req_ptr;
    CHECK_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);

    if (req->response_headers == NULL)
        return NULL;

    /* Simple header parsing */
    p = strcasestr(req->response_headers, name);
    if (p == NULL)
        return NULL;

    /* Skip to colon */
    p = strchr(p, ':');
    if (p == NULL)
        return NULL;
    p++;

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t')
        p++;

    /* Copy value */
    len = 0;
    while (*p != '\0' && *p != '\r' && *p != '\n' && len < MAX_HEADER_SIZE - 1)
        value[len++] = *p++;

    value[len] = '\0';

    return value;
}

/* Request object cache_hit */
VCL_INT v_matchproto_(td_rgw_vinyl_request_cache_hit)
vmod_req_cache_hit(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr) {
    struct vmod_rgw_vinyl_req *req;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(req_ptr);

    if (*req_ptr == NULL)
        return 0;

    req = *req_ptr;
    CHECK_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);

    return req->cache_hit;
}

/* Request object free */
VCL_VOID v_matchproto_(td_rgw_vinyl_request_free)
vmod_req_free(VRT_CTX, struct vmod_rgw_vinyl_req **req_ptr) {
    struct vmod_rgw_vinyl_req *req;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(req_ptr);

    if (*req_ptr == NULL)
        return;

    req = *req_ptr;
    CHECK_OBJ(req, VMOD_RGW_VINYL_REQ_MAGIC);

    vmod_req_fini(ctx, req);
    *req_ptr = NULL;
}
