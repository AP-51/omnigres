/*
 * Copyright (c) 2014-2016 DeNA Co., Ltd., Kazuho Oku, Fastly, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef h2o__probes_h
#define h2o__probes_h

#include "picotls.h"

#define H2O_LOG_SOCK(_name, _sock, _block)                                                                                         \
    do {                                                                                                                           \
        PTLS_LOG_DEFINE_POINT(h2o, _name, logpoint);                                                                               \
        uint32_t active = ptls_log_point_maybe_active(&logpoint);                                                                  \
        if (PTLS_LIKELY(active == 0))                                                                                              \
            break;                                                                                                                 \
        h2o_socket_t *sock_ = (_sock);                                                                                             \
        ptls_log_conn_state_t *conn_state = h2o_socket_log_state(sock_);                                                           \
        active &= ptls_log_conn_maybe_active(conn_state, (const char *(*)(void *))h2o_socket_get_ssl_server_name, sock_);          \
        if (PTLS_LIKELY(active == 0))                                                                                              \
            break;                                                                                                                 \
        PTLS_LOG__DO_LOG(h2o, _name, conn_state, (const char *(*)(void *))h2o_socket_get_ssl_server_name, sock_, 1, {              \
            PTLS_LOG_ELEMENT_PTR(sock, sock_);                                                                                     \
            do {                                                                                                                   \
                _block                                                                                                             \
            } while (0);                                                                                                           \
        });                                                                                                                        \
    } while (0)
#define H2O_LOG_CONN(_name, _conn, _block)                                                                                         \
    do {                                                                                                                           \
        PTLS_LOG_DEFINE_POINT(h2o, _name, logpoint);                                                                               \
        uint32_t active = ptls_log_point_maybe_active(&logpoint);                                                                  \
        if (active == 0)                                                                                                           \
            break;                                                                                                                 \
        h2o_conn_t *conn_ = (_conn);                                                                                               \
        ptls_log_conn_state_t *conn_state = conn_->callbacks->log_state(conn_);                                                    \
        active &= ptls_log_conn_maybe_active(conn_state, (const char *(*)(void *))conn_->callbacks->get_ssl_server_name, conn_);   \
        if (active == 0)                                                                                                           \
            break;                                                                                                                 \
        PTLS_LOG__DO_LOG(h2o, _name, conn_state, (const char *(*)(void *))conn_->callbacks->get_ssl_server_name, conn_, 1, {       \
            PTLS_LOG_ELEMENT_UNSIGNED(conn_id, conn_->id);                                                                         \
            do {                                                                                                                   \
                _block                                                                                                             \
            } while (0);                                                                                                           \
        });                                                                                                                        \
    } while (0)

/* This file is placed under lib, and must only be included from the source files of the h2o / libh2o, because H2O_USE_DTRACE is a
 * symbol available only during the build phase of h2o.  That's fine, because only h2o / libh2o has the sole right to define probes
 * belonging to the h2o namespace.
 */
#if H2O_USE_DTRACE

#include "picotls.h"
/* as probes_.h is used by files under lib/common, structures that are specific to the server-side implementation have to be
 * forward-declared. */
struct st_h2o_conn_t;
struct st_h2o_tunnel_t;
#include "h2o-probes.h"

#define H2O_PROBE_IS_ENABLED(label) (PTLS_UNLIKELY(H2O_##label##_ENABLED()))

#define H2O_PROBE_CONN0(label, conn)                                                                                               \
    do {                                                                                                                           \
        if (H2O_PROBE_IS_ENABLED(label)) {                                                                                         \
            H2O_##label((conn)->id);                                                                                               \
        }                                                                                                                          \
    } while (0)

#define H2O_PROBE_CONN(label, conn, ...)                                                                                           \
    do {                                                                                                                           \
        if (H2O_PROBE_IS_ENABLED(label)) {                                                                                         \
            H2O_##label((conn)->id, __VA_ARGS__);                                                                                  \
        }                                                                                                                          \
    } while (0)

#define H2O_PROBE_REQUEST0(label, req)                                                                                             \
    do {                                                                                                                           \
        if (H2O_PROBE_IS_ENABLED(label)) {                                                                                         \
            h2o_req_t *_req = (req);                                                                                               \
            h2o_conn_t *_conn = _req->conn;                                                                                        \
            uint64_t _req_id = _conn->callbacks->get_req_id(_req);                                                                 \
            H2O_##label(_conn->id, _req_id);                                                                                       \
        }                                                                                                                          \
    } while (0)

#define H2O_PROBE_REQUEST(label, req, ...)                                                                                         \
    do {                                                                                                                           \
        if (H2O_PROBE_IS_ENABLED(label)) {                                                                                         \
            h2o_req_t *_req = (req);                                                                                               \
            h2o_conn_t *_conn = _req->conn;                                                                                        \
            uint64_t _req_id = _conn->callbacks->get_req_id(_req);                                                                 \
            H2O_##label(_conn->id, _req_id, __VA_ARGS__);                                                                          \
        }                                                                                                                          \
    } while (0)

#define H2O_PROBE(label, ...)                                                                                                      \
    do {                                                                                                                           \
        if (H2O_PROBE_IS_ENABLED(label)) {                                                                                         \
            H2O_##label(__VA_ARGS__);                                                                                              \
        }                                                                                                                          \
    } while (0)

#define H2O_PROBE_HEXDUMP(s, l)                                                                                                    \
    ({                                                                                                                             \
        size_t _l = (l);                                                                                                           \
        ptls_hexdump(alloca(_l * 2 + 1), (s), _l);                                                                                 \
    })

#else

#define H2O_PROBE_IS_ENABLED(label) (0)
#define H2O_PROBE_CONN0(label, conn)
#define H2O_PROBE_CONN(label, conn, ...)
#define H2O_PROBE_REQUEST0(label, req)
#define H2O_PROBE_REQUEST(label, req, ...)
#define H2O_PROBE(label, ...)
#define H2O_PROBE_HEXDUMP(s, l)

#endif

/* Helper functions for probing; the functions are defined as non-inlineable, as bcc cannot handle relative offset against a static
 * const (e.g., H2O_TOKEN_PATH->buf.base). They are available only when h2o.h is included, so that files under lib/common can
 * include this function without creating dependency against lib/core (e.g., `h2o_req_t`). */
#ifdef h2o_h

__attribute__((noinline)) static void h2o_probe_request_header(h2o_req_t *req, uint64_t req_index, h2o_iovec_t name,
                                                               h2o_iovec_t value)
{
    H2O_PROBE_CONN(RECEIVE_REQUEST_HEADER, req->conn, req_index, name.base, name.len, value.base, value.len);
    H2O_LOG_CONN(receive_request_header, req->conn, {
        PTLS_LOG_ELEMENT_UNSIGNED(req_id, req_index);
        PTLS_LOG_APPDATA_ELEMENT_UNSAFESTR(name, name.base, name.len);
        PTLS_LOG_APPDATA_ELEMENT_UNSAFESTR(value, value.base, value.len);
    });
}

__attribute__((noinline)) static void h2o_probe_response_header(h2o_req_t *req, uint64_t req_index, h2o_iovec_t name,
                                                                h2o_iovec_t value)
{
    H2O_PROBE_CONN(SEND_RESPONSE_HEADER, req->conn, req_index, name.base, name.len, value.base, value.len);
    H2O_LOG_CONN(send_response_header, req->conn, {
        PTLS_LOG_ELEMENT_UNSIGNED(req_id, req_index);
        PTLS_LOG_APPDATA_ELEMENT_UNSAFESTR(name, name.base, name.len);
        PTLS_LOG_APPDATA_ELEMENT_UNSAFESTR(value, value.base, value.len);
    });
}

static inline void h2o_probe_log_request(h2o_req_t *req, uint64_t req_index)
{
    H2O_PROBE_CONN(RECEIVE_REQUEST, req->conn, req_index, req->version);
    H2O_LOG_CONN(receive_request, req->conn, {
        PTLS_LOG_ELEMENT_UNSIGNED(req_id, req_index);
        PTLS_LOG_ELEMENT_SIGNED(http_version, req->version);
    });

    PTLS_LOG_DEFINE_POINT(h2o, receive_request_header, receive_request_header_logpoint);
    if (H2O_PROBE_IS_ENABLED(RECEIVE_REQUEST_HEADER) ||
        (ptls_log_point_maybe_active(&receive_request_header_logpoint) != 0 &&
         (receive_request_header_logpoint.state.active_conns &
          ptls_log_conn_maybe_active(req->conn->callbacks->log_state(req->conn),
                                     (const char *(*)(void *))req->conn->callbacks->get_ssl_server_name, req->conn)) != 0)) {
        if (req->input.authority.base != NULL)
            h2o_probe_request_header(req, req_index, H2O_TOKEN_AUTHORITY->buf, req->input.authority);
        if (req->input.method.base != NULL)
            h2o_probe_request_header(req, req_index, H2O_TOKEN_METHOD->buf, req->input.method);
        if (req->input.path.base != NULL)
            h2o_probe_request_header(req, req_index, H2O_TOKEN_PATH->buf, req->input.path);
        if (req->input.scheme != NULL)
            h2o_probe_request_header(req, req_index, H2O_TOKEN_SCHEME->buf, req->input.scheme->name);
        size_t i;
        for (i = 0; i != req->headers.size; ++i) {
            h2o_header_t *h = req->headers.entries + i;
            h2o_probe_request_header(req, req_index, *h->name, h->value);
        }
    }
}

static inline void h2o_probe_log_response(h2o_req_t *req, uint64_t req_index)
{
    H2O_PROBE_CONN(SEND_RESPONSE, req->conn, req_index, req->res.status);
    H2O_LOG_CONN(send_response, req->conn, {
        PTLS_LOG_ELEMENT_UNSIGNED(req_id, req_index);
        PTLS_LOG_ELEMENT_SIGNED(status, req->res.status);
    });
    PTLS_LOG_DEFINE_POINT(h2o, send_response_header, send_response_header_logpoint);
    if (H2O_PROBE_IS_ENABLED(SEND_RESPONSE_HEADER) ||
        (ptls_log_point_maybe_active(&send_response_header_logpoint) != 0 &&
         (send_response_header_logpoint.state.active_conns &
          ptls_log_conn_maybe_active(req->conn->callbacks->log_state(req->conn),
                                     (const char *(*)(void *))req->conn->callbacks->get_ssl_server_name, req->conn)) != 0)) {
        if (req->res.content_length != SIZE_MAX) {
            char buf[sizeof(H2O_SIZE_T_LONGEST_STR)];
            size_t len = (size_t)sprintf(buf, "%zu", req->res.content_length);
            h2o_probe_response_header(req, req_index, H2O_TOKEN_CONTENT_LENGTH->buf, h2o_iovec_init(buf, len));
        }
        size_t i;
        for (i = 0; i != req->res.headers.size; ++i) {
            h2o_header_t *h = req->res.headers.entries + i;
            h2o_probe_response_header(req, req_index, *h->name, h->value);
        }
    }
}

#endif

#endif
