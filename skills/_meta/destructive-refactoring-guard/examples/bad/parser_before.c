/*
 * src/parser.c — INI-style configuration parser, v9.4
 *
 * 150 per-key validators accumulated over 6 years.
 * Each key range-checks, type-checks and error-reports its
 * value; unknown keys are rejected with a message.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>

typedef struct cfg_t cfg_t;

struct cfg_t {
    long server_bind_address;
    long server_bind_port;
    char server_backlog[256];
    long server_max_conns;
    long server_keepalive_timeout;
    long server_threads;
    long server_worker_affinity;
    char server_max_request_size[256];
    long server_read_timeout;
    long server_write_timeout;
    long server_graceful_shutdown;
    long server_ipv6_only;
    char server_socket_tx_buf[256];
    long server_socket_rx_buf;
    long server_http2_h2c;
    long tls_cert_file;
    long tls_key_file;
    char tls_ca_file[256];
    long tls_ciphers;
    long tls_min_version;
    long tls_session_cache;
    long tls_tickets;
    char tls_hsts[256];
    long tls_ocsp;
    long tls_client_auth;
    long tls_early_data;
    long tls_psk;
    char tls_keylog[256];
    long tls_session_timeout;
    long tls_renegotiate;
    long log_level;
    long log_file;
    char log_max_size[256];
    long log_rotate;
    long log_compress;
    long log_format;
    long log_timestamp;
    char log_pid[256];
    long log_thread_id;
    long log_line_num;
    long log_color;
    long log_json;
    char log_sync[256];
    long log_buffer_kb;
    long log_filter;
    long cache_size;
    long cache_policy;
    char cache_ttl[256];
    long cache_shards;
    long cache_write_through;
    long cache_max_entry;
    long cache_prefetch;
    char cache_invalidate[256];
    long cache_rehash;
    long cache_aligned;
    long cache_eviction;
    long cache_lock_stripes;
    char cache_cold_threshold[256];
    long cache_hit_ratio;
    long cache_bypass;
    long db_host;
    long db_port;
    char db_user[256];
    long db_password;
    long db_database;
    long db_pool_size;
    long db_connect_timeout;
    char db_idle_timeout[256];
    long db_ssl;
    long db_replica;
    long db_charset;
    long db_max_retries;
    char db_backoff[256];
    long db_read_only;
    long db_statement_cache;
    long net_listen_backlog;
    long net_nodelay;
    char net_quickack[256];
    long net_tcp_keepalive;
    long net_max_conn_per_ip;
    long net_send_buffer;
    long net_recv_buffer;
    char net_proxy_protocol[256];
    long net_reuse_port;
    long net_dscp;
    long net_tcp_keepidle;
    long net_tcp_keepintvl;
    char net_tcp_keepcnt[256];
    long net_backlog_accept;
    long net_tun_only;
    long security_key_rotation;
    long security_max_login_attempts;
    char security_lockout_seconds[256];
    long security_password_min_len;
    long security_password_min_class;
    long security_session_timeout;
    long security_token_ttl;
    char security_rate_limit[256];
    long security_allow_headers;
    long security_deny_ranges;
    long security_csp;
    long security_hsts_age;
    char security_secure_cookies[256];
    long security_same_site;
    long security_mfa;
    long mq_uri;
    long mq_reconnect;
    char mq_heartbeat[256];
    long mq_prefetch;
    long mq_ack_timeout;
    long mq_max_redeliver;
    long mq_dead_letter;
    char mq_max_batch[256];
    long mq_publish_confirm;
    long mq_consumer_threads;
    long mq_vhost;
    long mq_queue_ttl;
    char mq_flow_control[256];
    long mq_connection_timeout;
    long mq_tls_verify;
    long metrics_enabled;
    long metrics_endpoint;
    char metrics_interval[256];
    long metrics_retention;
    long metrics_aggregate;
    long metrics_labels;
    long metrics_histogram;
    char metrics_samples[256];
    long metrics_push_url;
    long metrics_token;
    long metrics_scrape_timeout;
    long metrics_chunk_size;
    char metrics_compression[256];
    long metrics_tls;
    long metrics_tags;
    long fuzz_seed;
    long fuzz_dict;
    char fuzz_max_input[256];
    long fuzz_runs;
    long fuzz_workers;
    long fuzz_timeout;
    long fuzz_artifact_dir;
    char fuzz_coverage[256];
    long fuzz_crash_retain;
    long fuzz_bisect;
    long fuzz_compare_exit;
    long fuzz_dedup;
    char fuzz_queue_depth[256];
    long fuzz_resume;
    long fuzz_verbosity;
};

static void cfg_error(cfg_t *cfg, const char *key,
                      const char *fmt, ...);

struct handler {
    const char *key;
    int (*fn)(const char *val, cfg_t *cfg);
};

/* ---- section: server ---- */

/* server.bind_address — integer value in [0, 1000000] */
static int h_server_bind_address(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "server.bind_address", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "server.bind_address", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->server_bind_address = v;
    return 0;
}

/* server.bind_port — boolean (1/0, true/false, yes/no, on/off) */
static int h_server_bind_port(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->server_bind_port = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->server_bind_port = 0;
        return 0;
    }
    cfg_error(cfg, "server.bind_port", "not a boolean: '%s'", val);
    return -1;
}

/* server.backlog — non-empty string, no '=', max 255 bytes */
static int h_server_backlog(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "server.backlog", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "server.backlog", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->server_backlog, sizeof(cfg->server_backlog), "%s", val);
    return 0;
}

/* server.max_conns — size with optional K/M/G suffix */
static int h_server_max_conns(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "server.max_conns", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->server_max_conns = (long)v;
    return 0;
}

/* server.keepalive_timeout — one of: fast, balanced, safe, aggressive */
static int h_server_keepalive_timeout(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->server_keepalive_timeout = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->server_keepalive_timeout = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->server_keepalive_timeout = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->server_keepalive_timeout = 3;
        return 0;
    }
    cfg_error(cfg, "server.keepalive_timeout", "unknown option: '%s'", val);
    return -1;
}

/* server.threads — integer value in [1, 65535] */
static int h_server_threads(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "server.threads", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "server.threads", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->server_threads = v;
    return 0;
}

/* server.worker_affinity — boolean (1/0, true/false, yes/no, on/off) */
static int h_server_worker_affinity(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->server_worker_affinity = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->server_worker_affinity = 0;
        return 0;
    }
    cfg_error(cfg, "server.worker_affinity", "not a boolean: '%s'", val);
    return -1;
}

/* server.max_request_size — non-empty string, no '=', max 255 bytes */
static int h_server_max_request_size(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "server.max_request_size", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "server.max_request_size", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->server_max_request_size, sizeof(cfg->server_max_request_size), "%s", val);
    return 0;
}

/* server.read_timeout — size with optional K/M/G suffix */
static int h_server_read_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "server.read_timeout", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->server_read_timeout = (long)v;
    return 0;
}

/* server.write_timeout — one of: fast, balanced, safe, aggressive */
static int h_server_write_timeout(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->server_write_timeout = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->server_write_timeout = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->server_write_timeout = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->server_write_timeout = 3;
        return 0;
    }
    cfg_error(cfg, "server.write_timeout", "unknown option: '%s'", val);
    return -1;
}

/* server.graceful_shutdown — integer value in [1, 65535] */
static int h_server_graceful_shutdown(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "server.graceful_shutdown", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "server.graceful_shutdown", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->server_graceful_shutdown = v;
    return 0;
}

/* server.ipv6_only — boolean (1/0, true/false, yes/no, on/off) */
static int h_server_ipv6_only(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->server_ipv6_only = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->server_ipv6_only = 0;
        return 0;
    }
    cfg_error(cfg, "server.ipv6_only", "not a boolean: '%s'", val);
    return -1;
}

/* server.socket_tx_buf — non-empty string, no '=', max 255 bytes */
static int h_server_socket_tx_buf(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "server.socket_tx_buf", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "server.socket_tx_buf", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->server_socket_tx_buf, sizeof(cfg->server_socket_tx_buf), "%s", val);
    return 0;
}

/* server.socket_rx_buf — size with optional K/M/G suffix */
static int h_server_socket_rx_buf(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "server.socket_rx_buf", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->server_socket_rx_buf = (long)v;
    return 0;
}

/* server.http2_h2c — one of: fast, balanced, safe, aggressive */
static int h_server_http2_h2c(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->server_http2_h2c = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->server_http2_h2c = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->server_http2_h2c = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->server_http2_h2c = 3;
        return 0;
    }
    cfg_error(cfg, "server.http2_h2c", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: tls ---- */

/* tls.cert_file — integer value in [0, 1000000] */
static int h_tls_cert_file(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "tls.cert_file", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "tls.cert_file", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->tls_cert_file = v;
    return 0;
}

/* tls.key_file — boolean (1/0, true/false, yes/no, on/off) */
static int h_tls_key_file(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->tls_key_file = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->tls_key_file = 0;
        return 0;
    }
    cfg_error(cfg, "tls.key_file", "not a boolean: '%s'", val);
    return -1;
}

/* tls.ca_file — non-empty string, no '=', max 255 bytes */
static int h_tls_ca_file(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "tls.ca_file", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "tls.ca_file", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->tls_ca_file, sizeof(cfg->tls_ca_file), "%s", val);
    return 0;
}

/* tls.ciphers — size with optional K/M/G suffix */
static int h_tls_ciphers(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "tls.ciphers", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->tls_ciphers = (long)v;
    return 0;
}

/* tls.min_version — one of: fast, balanced, safe, aggressive */
static int h_tls_min_version(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->tls_min_version = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->tls_min_version = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->tls_min_version = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->tls_min_version = 3;
        return 0;
    }
    cfg_error(cfg, "tls.min_version", "unknown option: '%s'", val);
    return -1;
}

/* tls.session_cache — integer value in [1, 65535] */
static int h_tls_session_cache(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "tls.session_cache", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "tls.session_cache", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->tls_session_cache = v;
    return 0;
}

/* tls.tickets — boolean (1/0, true/false, yes/no, on/off) */
static int h_tls_tickets(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->tls_tickets = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->tls_tickets = 0;
        return 0;
    }
    cfg_error(cfg, "tls.tickets", "not a boolean: '%s'", val);
    return -1;
}

/* tls.hsts — non-empty string, no '=', max 255 bytes */
static int h_tls_hsts(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "tls.hsts", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "tls.hsts", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->tls_hsts, sizeof(cfg->tls_hsts), "%s", val);
    return 0;
}

/* tls.ocsp — size with optional K/M/G suffix */
static int h_tls_ocsp(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "tls.ocsp", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->tls_ocsp = (long)v;
    return 0;
}

/* tls.client_auth — one of: fast, balanced, safe, aggressive */
static int h_tls_client_auth(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->tls_client_auth = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->tls_client_auth = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->tls_client_auth = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->tls_client_auth = 3;
        return 0;
    }
    cfg_error(cfg, "tls.client_auth", "unknown option: '%s'", val);
    return -1;
}

/* tls.early_data — integer value in [1, 65535] */
static int h_tls_early_data(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "tls.early_data", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "tls.early_data", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->tls_early_data = v;
    return 0;
}

/* tls.psk — boolean (1/0, true/false, yes/no, on/off) */
static int h_tls_psk(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->tls_psk = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->tls_psk = 0;
        return 0;
    }
    cfg_error(cfg, "tls.psk", "not a boolean: '%s'", val);
    return -1;
}

/* tls.keylog — non-empty string, no '=', max 255 bytes */
static int h_tls_keylog(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "tls.keylog", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "tls.keylog", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->tls_keylog, sizeof(cfg->tls_keylog), "%s", val);
    return 0;
}

/* tls.session_timeout — size with optional K/M/G suffix */
static int h_tls_session_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "tls.session_timeout", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->tls_session_timeout = (long)v;
    return 0;
}

/* tls.renegotiate — one of: fast, balanced, safe, aggressive */
static int h_tls_renegotiate(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->tls_renegotiate = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->tls_renegotiate = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->tls_renegotiate = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->tls_renegotiate = 3;
        return 0;
    }
    cfg_error(cfg, "tls.renegotiate", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: log ---- */

/* log.level — integer value in [0, 1000000] */
static int h_log_level(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "log.level", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "log.level", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->log_level = v;
    return 0;
}

/* log.file — boolean (1/0, true/false, yes/no, on/off) */
static int h_log_file(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->log_file = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->log_file = 0;
        return 0;
    }
    cfg_error(cfg, "log.file", "not a boolean: '%s'", val);
    return -1;
}

/* log.max_size — non-empty string, no '=', max 255 bytes */
static int h_log_max_size(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "log.max_size", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "log.max_size", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->log_max_size, sizeof(cfg->log_max_size), "%s", val);
    return 0;
}

/* log.rotate — size with optional K/M/G suffix */
static int h_log_rotate(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "log.rotate", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->log_rotate = (long)v;
    return 0;
}

/* log.compress — one of: fast, balanced, safe, aggressive */
static int h_log_compress(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->log_compress = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->log_compress = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->log_compress = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->log_compress = 3;
        return 0;
    }
    cfg_error(cfg, "log.compress", "unknown option: '%s'", val);
    return -1;
}

/* log.format — integer value in [1, 65535] */
static int h_log_format(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "log.format", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "log.format", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->log_format = v;
    return 0;
}

/* log.timestamp — boolean (1/0, true/false, yes/no, on/off) */
static int h_log_timestamp(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->log_timestamp = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->log_timestamp = 0;
        return 0;
    }
    cfg_error(cfg, "log.timestamp", "not a boolean: '%s'", val);
    return -1;
}

/* log.pid — non-empty string, no '=', max 255 bytes */
static int h_log_pid(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "log.pid", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "log.pid", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->log_pid, sizeof(cfg->log_pid), "%s", val);
    return 0;
}

/* log.thread_id — size with optional K/M/G suffix */
static int h_log_thread_id(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "log.thread_id", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->log_thread_id = (long)v;
    return 0;
}

/* log.line_num — one of: fast, balanced, safe, aggressive */
static int h_log_line_num(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->log_line_num = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->log_line_num = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->log_line_num = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->log_line_num = 3;
        return 0;
    }
    cfg_error(cfg, "log.line_num", "unknown option: '%s'", val);
    return -1;
}

/* log.color — integer value in [1, 65535] */
static int h_log_color(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "log.color", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "log.color", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->log_color = v;
    return 0;
}

/* log.json — boolean (1/0, true/false, yes/no, on/off) */
static int h_log_json(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->log_json = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->log_json = 0;
        return 0;
    }
    cfg_error(cfg, "log.json", "not a boolean: '%s'", val);
    return -1;
}

/* log.sync — non-empty string, no '=', max 255 bytes */
static int h_log_sync(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "log.sync", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "log.sync", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->log_sync, sizeof(cfg->log_sync), "%s", val);
    return 0;
}

/* log.buffer_kb — size with optional K/M/G suffix */
static int h_log_buffer_kb(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "log.buffer_kb", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->log_buffer_kb = (long)v;
    return 0;
}

/* log.filter — one of: fast, balanced, safe, aggressive */
static int h_log_filter(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->log_filter = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->log_filter = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->log_filter = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->log_filter = 3;
        return 0;
    }
    cfg_error(cfg, "log.filter", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: cache ---- */

/* cache.size — integer value in [0, 1000000] */
static int h_cache_size(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "cache.size", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "cache.size", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->cache_size = v;
    return 0;
}

/* cache.policy — boolean (1/0, true/false, yes/no, on/off) */
static int h_cache_policy(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->cache_policy = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->cache_policy = 0;
        return 0;
    }
    cfg_error(cfg, "cache.policy", "not a boolean: '%s'", val);
    return -1;
}

/* cache.ttl — non-empty string, no '=', max 255 bytes */
static int h_cache_ttl(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "cache.ttl", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "cache.ttl", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->cache_ttl, sizeof(cfg->cache_ttl), "%s", val);
    return 0;
}

/* cache.shards — size with optional K/M/G suffix */
static int h_cache_shards(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "cache.shards", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->cache_shards = (long)v;
    return 0;
}

/* cache.write_through — one of: fast, balanced, safe, aggressive */
static int h_cache_write_through(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->cache_write_through = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->cache_write_through = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->cache_write_through = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->cache_write_through = 3;
        return 0;
    }
    cfg_error(cfg, "cache.write_through", "unknown option: '%s'", val);
    return -1;
}

/* cache.max_entry — integer value in [1, 65535] */
static int h_cache_max_entry(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "cache.max_entry", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "cache.max_entry", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->cache_max_entry = v;
    return 0;
}

/* cache.prefetch — boolean (1/0, true/false, yes/no, on/off) */
static int h_cache_prefetch(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->cache_prefetch = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->cache_prefetch = 0;
        return 0;
    }
    cfg_error(cfg, "cache.prefetch", "not a boolean: '%s'", val);
    return -1;
}

/* cache.invalidate — non-empty string, no '=', max 255 bytes */
static int h_cache_invalidate(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "cache.invalidate", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "cache.invalidate", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->cache_invalidate, sizeof(cfg->cache_invalidate), "%s", val);
    return 0;
}

/* cache.rehash — size with optional K/M/G suffix */
static int h_cache_rehash(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "cache.rehash", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->cache_rehash = (long)v;
    return 0;
}

/* cache.aligned — one of: fast, balanced, safe, aggressive */
static int h_cache_aligned(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->cache_aligned = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->cache_aligned = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->cache_aligned = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->cache_aligned = 3;
        return 0;
    }
    cfg_error(cfg, "cache.aligned", "unknown option: '%s'", val);
    return -1;
}

/* cache.eviction — integer value in [1, 65535] */
static int h_cache_eviction(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "cache.eviction", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "cache.eviction", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->cache_eviction = v;
    return 0;
}

/* cache.lock_stripes — boolean (1/0, true/false, yes/no, on/off) */
static int h_cache_lock_stripes(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->cache_lock_stripes = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->cache_lock_stripes = 0;
        return 0;
    }
    cfg_error(cfg, "cache.lock_stripes", "not a boolean: '%s'", val);
    return -1;
}

/* cache.cold_threshold — non-empty string, no '=', max 255 bytes */
static int h_cache_cold_threshold(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "cache.cold_threshold", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "cache.cold_threshold", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->cache_cold_threshold, sizeof(cfg->cache_cold_threshold), "%s", val);
    return 0;
}

/* cache.hit_ratio — size with optional K/M/G suffix */
static int h_cache_hit_ratio(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "cache.hit_ratio", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->cache_hit_ratio = (long)v;
    return 0;
}

/* cache.bypass — one of: fast, balanced, safe, aggressive */
static int h_cache_bypass(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->cache_bypass = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->cache_bypass = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->cache_bypass = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->cache_bypass = 3;
        return 0;
    }
    cfg_error(cfg, "cache.bypass", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: db ---- */

/* db.host — integer value in [0, 1000000] */
static int h_db_host(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "db.host", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "db.host", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->db_host = v;
    return 0;
}

/* db.port — boolean (1/0, true/false, yes/no, on/off) */
static int h_db_port(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->db_port = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->db_port = 0;
        return 0;
    }
    cfg_error(cfg, "db.port", "not a boolean: '%s'", val);
    return -1;
}

/* db.user — non-empty string, no '=', max 255 bytes */
static int h_db_user(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "db.user", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "db.user", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->db_user, sizeof(cfg->db_user), "%s", val);
    return 0;
}

/* db.password — size with optional K/M/G suffix */
static int h_db_password(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "db.password", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->db_password = (long)v;
    return 0;
}

/* db.database — one of: fast, balanced, safe, aggressive */
static int h_db_database(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->db_database = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->db_database = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->db_database = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->db_database = 3;
        return 0;
    }
    cfg_error(cfg, "db.database", "unknown option: '%s'", val);
    return -1;
}

/* db.pool_size — integer value in [1, 65535] */
static int h_db_pool_size(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "db.pool_size", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "db.pool_size", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->db_pool_size = v;
    return 0;
}

/* db.connect_timeout — boolean (1/0, true/false, yes/no, on/off) */
static int h_db_connect_timeout(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->db_connect_timeout = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->db_connect_timeout = 0;
        return 0;
    }
    cfg_error(cfg, "db.connect_timeout", "not a boolean: '%s'", val);
    return -1;
}

/* db.idle_timeout — non-empty string, no '=', max 255 bytes */
static int h_db_idle_timeout(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "db.idle_timeout", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "db.idle_timeout", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->db_idle_timeout, sizeof(cfg->db_idle_timeout), "%s", val);
    return 0;
}

/* db.ssl — size with optional K/M/G suffix */
static int h_db_ssl(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "db.ssl", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->db_ssl = (long)v;
    return 0;
}

/* db.replica — one of: fast, balanced, safe, aggressive */
static int h_db_replica(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->db_replica = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->db_replica = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->db_replica = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->db_replica = 3;
        return 0;
    }
    cfg_error(cfg, "db.replica", "unknown option: '%s'", val);
    return -1;
}

/* db.charset — integer value in [1, 65535] */
static int h_db_charset(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "db.charset", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "db.charset", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->db_charset = v;
    return 0;
}

/* db.max_retries — boolean (1/0, true/false, yes/no, on/off) */
static int h_db_max_retries(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->db_max_retries = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->db_max_retries = 0;
        return 0;
    }
    cfg_error(cfg, "db.max_retries", "not a boolean: '%s'", val);
    return -1;
}

/* db.backoff — non-empty string, no '=', max 255 bytes */
static int h_db_backoff(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "db.backoff", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "db.backoff", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->db_backoff, sizeof(cfg->db_backoff), "%s", val);
    return 0;
}

/* db.read_only — size with optional K/M/G suffix */
static int h_db_read_only(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "db.read_only", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->db_read_only = (long)v;
    return 0;
}

/* db.statement_cache — one of: fast, balanced, safe, aggressive */
static int h_db_statement_cache(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->db_statement_cache = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->db_statement_cache = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->db_statement_cache = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->db_statement_cache = 3;
        return 0;
    }
    cfg_error(cfg, "db.statement_cache", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: net ---- */

/* net.listen_backlog — integer value in [0, 1000000] */
static int h_net_listen_backlog(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "net.listen_backlog", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "net.listen_backlog", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->net_listen_backlog = v;
    return 0;
}

/* net.nodelay — boolean (1/0, true/false, yes/no, on/off) */
static int h_net_nodelay(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->net_nodelay = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->net_nodelay = 0;
        return 0;
    }
    cfg_error(cfg, "net.nodelay", "not a boolean: '%s'", val);
    return -1;
}

/* net.quickack — non-empty string, no '=', max 255 bytes */
static int h_net_quickack(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "net.quickack", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "net.quickack", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->net_quickack, sizeof(cfg->net_quickack), "%s", val);
    return 0;
}

/* net.tcp_keepalive — size with optional K/M/G suffix */
static int h_net_tcp_keepalive(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "net.tcp_keepalive", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->net_tcp_keepalive = (long)v;
    return 0;
}

/* net.max_conn_per_ip — one of: fast, balanced, safe, aggressive */
static int h_net_max_conn_per_ip(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->net_max_conn_per_ip = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->net_max_conn_per_ip = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->net_max_conn_per_ip = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->net_max_conn_per_ip = 3;
        return 0;
    }
    cfg_error(cfg, "net.max_conn_per_ip", "unknown option: '%s'", val);
    return -1;
}

/* net.send_buffer — integer value in [1, 65535] */
static int h_net_send_buffer(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "net.send_buffer", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "net.send_buffer", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->net_send_buffer = v;
    return 0;
}

/* net.recv_buffer — boolean (1/0, true/false, yes/no, on/off) */
static int h_net_recv_buffer(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->net_recv_buffer = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->net_recv_buffer = 0;
        return 0;
    }
    cfg_error(cfg, "net.recv_buffer", "not a boolean: '%s'", val);
    return -1;
}

/* net.proxy_protocol — non-empty string, no '=', max 255 bytes */
static int h_net_proxy_protocol(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "net.proxy_protocol", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "net.proxy_protocol", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->net_proxy_protocol, sizeof(cfg->net_proxy_protocol), "%s", val);
    return 0;
}

/* net.reuse_port — size with optional K/M/G suffix */
static int h_net_reuse_port(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "net.reuse_port", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->net_reuse_port = (long)v;
    return 0;
}

/* net.dscp — one of: fast, balanced, safe, aggressive */
static int h_net_dscp(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->net_dscp = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->net_dscp = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->net_dscp = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->net_dscp = 3;
        return 0;
    }
    cfg_error(cfg, "net.dscp", "unknown option: '%s'", val);
    return -1;
}

/* net.tcp_keepidle — integer value in [1, 65535] */
static int h_net_tcp_keepidle(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "net.tcp_keepidle", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "net.tcp_keepidle", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->net_tcp_keepidle = v;
    return 0;
}

/* net.tcp_keepintvl — boolean (1/0, true/false, yes/no, on/off) */
static int h_net_tcp_keepintvl(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->net_tcp_keepintvl = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->net_tcp_keepintvl = 0;
        return 0;
    }
    cfg_error(cfg, "net.tcp_keepintvl", "not a boolean: '%s'", val);
    return -1;
}

/* net.tcp_keepcnt — non-empty string, no '=', max 255 bytes */
static int h_net_tcp_keepcnt(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "net.tcp_keepcnt", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "net.tcp_keepcnt", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->net_tcp_keepcnt, sizeof(cfg->net_tcp_keepcnt), "%s", val);
    return 0;
}

/* net.backlog_accept — size with optional K/M/G suffix */
static int h_net_backlog_accept(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "net.backlog_accept", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->net_backlog_accept = (long)v;
    return 0;
}

/* net.tun_only — one of: fast, balanced, safe, aggressive */
static int h_net_tun_only(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->net_tun_only = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->net_tun_only = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->net_tun_only = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->net_tun_only = 3;
        return 0;
    }
    cfg_error(cfg, "net.tun_only", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: security ---- */

/* security.key_rotation — integer value in [0, 1000000] */
static int h_security_key_rotation(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "security.key_rotation", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "security.key_rotation", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->security_key_rotation = v;
    return 0;
}

/* security.max_login_attempts — boolean (1/0, true/false, yes/no, on/off) */
static int h_security_max_login_attempts(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->security_max_login_attempts = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->security_max_login_attempts = 0;
        return 0;
    }
    cfg_error(cfg, "security.max_login_attempts", "not a boolean: '%s'", val);
    return -1;
}

/* security.lockout_seconds — non-empty string, no '=', max 255 bytes */
static int h_security_lockout_seconds(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "security.lockout_seconds", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "security.lockout_seconds", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->security_lockout_seconds, sizeof(cfg->security_lockout_seconds), "%s", val);
    return 0;
}

/* security.password_min_len — size with optional K/M/G suffix */
static int h_security_password_min_len(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "security.password_min_len", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->security_password_min_len = (long)v;
    return 0;
}

/* security.password_min_class — one of: fast, balanced, safe, aggressive */
static int h_security_password_min_class(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->security_password_min_class = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->security_password_min_class = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->security_password_min_class = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->security_password_min_class = 3;
        return 0;
    }
    cfg_error(cfg, "security.password_min_class", "unknown option: '%s'", val);
    return -1;
}

/* security.session_timeout — integer value in [1, 65535] */
static int h_security_session_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "security.session_timeout", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "security.session_timeout", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->security_session_timeout = v;
    return 0;
}

/* security.token_ttl — boolean (1/0, true/false, yes/no, on/off) */
static int h_security_token_ttl(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->security_token_ttl = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->security_token_ttl = 0;
        return 0;
    }
    cfg_error(cfg, "security.token_ttl", "not a boolean: '%s'", val);
    return -1;
}

/* security.rate_limit — non-empty string, no '=', max 255 bytes */
static int h_security_rate_limit(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "security.rate_limit", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "security.rate_limit", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->security_rate_limit, sizeof(cfg->security_rate_limit), "%s", val);
    return 0;
}

/* security.allow_headers — size with optional K/M/G suffix */
static int h_security_allow_headers(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "security.allow_headers", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->security_allow_headers = (long)v;
    return 0;
}

/* security.deny_ranges — one of: fast, balanced, safe, aggressive */
static int h_security_deny_ranges(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->security_deny_ranges = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->security_deny_ranges = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->security_deny_ranges = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->security_deny_ranges = 3;
        return 0;
    }
    cfg_error(cfg, "security.deny_ranges", "unknown option: '%s'", val);
    return -1;
}

/* security.csp — integer value in [1, 65535] */
static int h_security_csp(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "security.csp", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "security.csp", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->security_csp = v;
    return 0;
}

/* security.hsts_age — boolean (1/0, true/false, yes/no, on/off) */
static int h_security_hsts_age(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->security_hsts_age = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->security_hsts_age = 0;
        return 0;
    }
    cfg_error(cfg, "security.hsts_age", "not a boolean: '%s'", val);
    return -1;
}

/* security.secure_cookies — non-empty string, no '=', max 255 bytes */
static int h_security_secure_cookies(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "security.secure_cookies", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "security.secure_cookies", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->security_secure_cookies, sizeof(cfg->security_secure_cookies), "%s", val);
    return 0;
}

/* security.same_site — size with optional K/M/G suffix */
static int h_security_same_site(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "security.same_site", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->security_same_site = (long)v;
    return 0;
}

/* security.mfa — one of: fast, balanced, safe, aggressive */
static int h_security_mfa(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->security_mfa = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->security_mfa = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->security_mfa = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->security_mfa = 3;
        return 0;
    }
    cfg_error(cfg, "security.mfa", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: mq ---- */

/* mq.uri — integer value in [0, 1000000] */
static int h_mq_uri(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "mq.uri", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "mq.uri", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->mq_uri = v;
    return 0;
}

/* mq.reconnect — boolean (1/0, true/false, yes/no, on/off) */
static int h_mq_reconnect(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->mq_reconnect = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->mq_reconnect = 0;
        return 0;
    }
    cfg_error(cfg, "mq.reconnect", "not a boolean: '%s'", val);
    return -1;
}

/* mq.heartbeat — non-empty string, no '=', max 255 bytes */
static int h_mq_heartbeat(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "mq.heartbeat", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "mq.heartbeat", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->mq_heartbeat, sizeof(cfg->mq_heartbeat), "%s", val);
    return 0;
}

/* mq.prefetch — size with optional K/M/G suffix */
static int h_mq_prefetch(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "mq.prefetch", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->mq_prefetch = (long)v;
    return 0;
}

/* mq.ack_timeout — one of: fast, balanced, safe, aggressive */
static int h_mq_ack_timeout(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->mq_ack_timeout = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->mq_ack_timeout = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->mq_ack_timeout = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->mq_ack_timeout = 3;
        return 0;
    }
    cfg_error(cfg, "mq.ack_timeout", "unknown option: '%s'", val);
    return -1;
}

/* mq.max_redeliver — integer value in [1, 65535] */
static int h_mq_max_redeliver(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "mq.max_redeliver", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "mq.max_redeliver", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->mq_max_redeliver = v;
    return 0;
}

/* mq.dead_letter — boolean (1/0, true/false, yes/no, on/off) */
static int h_mq_dead_letter(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->mq_dead_letter = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->mq_dead_letter = 0;
        return 0;
    }
    cfg_error(cfg, "mq.dead_letter", "not a boolean: '%s'", val);
    return -1;
}

/* mq.max_batch — non-empty string, no '=', max 255 bytes */
static int h_mq_max_batch(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "mq.max_batch", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "mq.max_batch", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->mq_max_batch, sizeof(cfg->mq_max_batch), "%s", val);
    return 0;
}

/* mq.publish_confirm — size with optional K/M/G suffix */
static int h_mq_publish_confirm(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "mq.publish_confirm", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->mq_publish_confirm = (long)v;
    return 0;
}

/* mq.consumer_threads — one of: fast, balanced, safe, aggressive */
static int h_mq_consumer_threads(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->mq_consumer_threads = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->mq_consumer_threads = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->mq_consumer_threads = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->mq_consumer_threads = 3;
        return 0;
    }
    cfg_error(cfg, "mq.consumer_threads", "unknown option: '%s'", val);
    return -1;
}

/* mq.vhost — integer value in [1, 65535] */
static int h_mq_vhost(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "mq.vhost", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "mq.vhost", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->mq_vhost = v;
    return 0;
}

/* mq.queue_ttl — boolean (1/0, true/false, yes/no, on/off) */
static int h_mq_queue_ttl(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->mq_queue_ttl = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->mq_queue_ttl = 0;
        return 0;
    }
    cfg_error(cfg, "mq.queue_ttl", "not a boolean: '%s'", val);
    return -1;
}

/* mq.flow_control — non-empty string, no '=', max 255 bytes */
static int h_mq_flow_control(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "mq.flow_control", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "mq.flow_control", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->mq_flow_control, sizeof(cfg->mq_flow_control), "%s", val);
    return 0;
}

/* mq.connection_timeout — size with optional K/M/G suffix */
static int h_mq_connection_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "mq.connection_timeout", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->mq_connection_timeout = (long)v;
    return 0;
}

/* mq.tls_verify — one of: fast, balanced, safe, aggressive */
static int h_mq_tls_verify(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->mq_tls_verify = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->mq_tls_verify = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->mq_tls_verify = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->mq_tls_verify = 3;
        return 0;
    }
    cfg_error(cfg, "mq.tls_verify", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: metrics ---- */

/* metrics.enabled — integer value in [0, 1000000] */
static int h_metrics_enabled(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "metrics.enabled", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "metrics.enabled", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->metrics_enabled = v;
    return 0;
}

/* metrics.endpoint — boolean (1/0, true/false, yes/no, on/off) */
static int h_metrics_endpoint(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->metrics_endpoint = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->metrics_endpoint = 0;
        return 0;
    }
    cfg_error(cfg, "metrics.endpoint", "not a boolean: '%s'", val);
    return -1;
}

/* metrics.interval — non-empty string, no '=', max 255 bytes */
static int h_metrics_interval(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "metrics.interval", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "metrics.interval", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->metrics_interval, sizeof(cfg->metrics_interval), "%s", val);
    return 0;
}

/* metrics.retention — size with optional K/M/G suffix */
static int h_metrics_retention(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "metrics.retention", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->metrics_retention = (long)v;
    return 0;
}

/* metrics.aggregate — one of: fast, balanced, safe, aggressive */
static int h_metrics_aggregate(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->metrics_aggregate = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->metrics_aggregate = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->metrics_aggregate = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->metrics_aggregate = 3;
        return 0;
    }
    cfg_error(cfg, "metrics.aggregate", "unknown option: '%s'", val);
    return -1;
}

/* metrics.labels — integer value in [1, 65535] */
static int h_metrics_labels(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "metrics.labels", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "metrics.labels", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->metrics_labels = v;
    return 0;
}

/* metrics.histogram — boolean (1/0, true/false, yes/no, on/off) */
static int h_metrics_histogram(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->metrics_histogram = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->metrics_histogram = 0;
        return 0;
    }
    cfg_error(cfg, "metrics.histogram", "not a boolean: '%s'", val);
    return -1;
}

/* metrics.samples — non-empty string, no '=', max 255 bytes */
static int h_metrics_samples(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "metrics.samples", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "metrics.samples", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->metrics_samples, sizeof(cfg->metrics_samples), "%s", val);
    return 0;
}

/* metrics.push_url — size with optional K/M/G suffix */
static int h_metrics_push_url(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "metrics.push_url", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->metrics_push_url = (long)v;
    return 0;
}

/* metrics.token — one of: fast, balanced, safe, aggressive */
static int h_metrics_token(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->metrics_token = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->metrics_token = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->metrics_token = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->metrics_token = 3;
        return 0;
    }
    cfg_error(cfg, "metrics.token", "unknown option: '%s'", val);
    return -1;
}

/* metrics.scrape_timeout — integer value in [1, 65535] */
static int h_metrics_scrape_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "metrics.scrape_timeout", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "metrics.scrape_timeout", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->metrics_scrape_timeout = v;
    return 0;
}

/* metrics.chunk_size — boolean (1/0, true/false, yes/no, on/off) */
static int h_metrics_chunk_size(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->metrics_chunk_size = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->metrics_chunk_size = 0;
        return 0;
    }
    cfg_error(cfg, "metrics.chunk_size", "not a boolean: '%s'", val);
    return -1;
}

/* metrics.compression — non-empty string, no '=', max 255 bytes */
static int h_metrics_compression(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "metrics.compression", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "metrics.compression", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->metrics_compression, sizeof(cfg->metrics_compression), "%s", val);
    return 0;
}

/* metrics.tls — size with optional K/M/G suffix */
static int h_metrics_tls(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "metrics.tls", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->metrics_tls = (long)v;
    return 0;
}

/* metrics.tags — one of: fast, balanced, safe, aggressive */
static int h_metrics_tags(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->metrics_tags = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->metrics_tags = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->metrics_tags = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->metrics_tags = 3;
        return 0;
    }
    cfg_error(cfg, "metrics.tags", "unknown option: '%s'", val);
    return -1;
}

/* ---- section: fuzz ---- */

/* fuzz.seed — integer value in [0, 1000000] */
static int h_fuzz_seed(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "fuzz.seed", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 0 || v > 1000000) {
        cfg_error(cfg, "fuzz.seed", "out of range %d..%d: %ld", 0, 1000000, v);
        return -1;
    }
    cfg->fuzz_seed = v;
    return 0;
}

/* fuzz.dict — boolean (1/0, true/false, yes/no, on/off) */
static int h_fuzz_dict(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->fuzz_dict = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->fuzz_dict = 0;
        return 0;
    }
    cfg_error(cfg, "fuzz.dict", "not a boolean: '%s'", val);
    return -1;
}

/* fuzz.max_input — non-empty string, no '=', max 255 bytes */
static int h_fuzz_max_input(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "fuzz.max_input", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "fuzz.max_input", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->fuzz_max_input, sizeof(cfg->fuzz_max_input), "%s", val);
    return 0;
}

/* fuzz.runs — size with optional K/M/G suffix */
static int h_fuzz_runs(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "fuzz.runs", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->fuzz_runs = (long)v;
    return 0;
}

/* fuzz.workers — one of: fast, balanced, safe, aggressive */
static int h_fuzz_workers(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->fuzz_workers = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->fuzz_workers = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->fuzz_workers = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->fuzz_workers = 3;
        return 0;
    }
    cfg_error(cfg, "fuzz.workers", "unknown option: '%s'", val);
    return -1;
}

/* fuzz.timeout — integer value in [1, 65535] */
static int h_fuzz_timeout(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "fuzz.timeout", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "fuzz.timeout", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->fuzz_timeout = v;
    return 0;
}

/* fuzz.artifact_dir — boolean (1/0, true/false, yes/no, on/off) */
static int h_fuzz_artifact_dir(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->fuzz_artifact_dir = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->fuzz_artifact_dir = 0;
        return 0;
    }
    cfg_error(cfg, "fuzz.artifact_dir", "not a boolean: '%s'", val);
    return -1;
}

/* fuzz.coverage — non-empty string, no '=', max 255 bytes */
static int h_fuzz_coverage(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "fuzz.coverage", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "fuzz.coverage", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->fuzz_coverage, sizeof(cfg->fuzz_coverage), "%s", val);
    return 0;
}

/* fuzz.crash_retain — size with optional K/M/G suffix */
static int h_fuzz_crash_retain(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "fuzz.crash_retain", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->fuzz_crash_retain = (long)v;
    return 0;
}

/* fuzz.bisect — one of: fast, balanced, safe, aggressive */
static int h_fuzz_bisect(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->fuzz_bisect = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->fuzz_bisect = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->fuzz_bisect = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->fuzz_bisect = 3;
        return 0;
    }
    cfg_error(cfg, "fuzz.bisect", "unknown option: '%s'", val);
    return -1;
}

/* fuzz.compare_exit — integer value in [1, 65535] */
static int h_fuzz_compare_exit(const char *val, cfg_t *cfg) {
    char *end = NULL;
    long v = strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        cfg_error(cfg, "fuzz.compare_exit", "not an integer: '%s'", val);
        return -1;
    }
    if (v < 1 || v > 65535) {
        cfg_error(cfg, "fuzz.compare_exit", "out of range %d..%d: %ld", 1, 65535, v);
        return -1;
    }
    cfg->fuzz_compare_exit = v;
    return 0;
}

/* fuzz.dedup — boolean (1/0, true/false, yes/no, on/off) */
static int h_fuzz_dedup(const char *val, cfg_t *cfg) {
    if (strcmp(val, "1") == 0 || strcmp(val, "true") == 0 ||
        strcmp(val, "yes") == 0 || strcmp(val, "on") == 0) {
        cfg->fuzz_dedup = 1;
        return 0;
    }
    if (strcmp(val, "0") == 0 || strcmp(val, "false") == 0 ||
        strcmp(val, "no") == 0 || strcmp(val, "off") == 0) {
        cfg->fuzz_dedup = 0;
        return 0;
    }
    cfg_error(cfg, "fuzz.dedup", "not a boolean: '%s'", val);
    return -1;
}

/* fuzz.queue_depth — non-empty string, no '=', max 255 bytes */
static int h_fuzz_queue_depth(const char *val, cfg_t *cfg) {
    size_t n = strlen(val);
    if (n == 0 || n > 255) {
        cfg_error(cfg, "fuzz.queue_depth", "length %zu out of 1..255", n);
        return -1;
    }
    if (strchr(val, '=') != NULL) {
        cfg_error(cfg, "fuzz.queue_depth", "value contains '='", val);
        return -1;
    }
    snprintf(cfg->fuzz_queue_depth, sizeof(cfg->fuzz_queue_depth), "%s", val);
    return 0;
}

/* fuzz.resume — size with optional K/M/G suffix */
static int h_fuzz_resume(const char *val, cfg_t *cfg) {
    char *end = NULL;
    unsigned long v = strtoul(val, &end, 10);
    if (end == val || *end == '\0') {
        cfg_error(cfg, "fuzz.resume", "missing size suffix: '%s'", val);
        return -1;
    }
    switch (*end) {
    case 'k': case 'K': v *= 1024UL; break;
    case 'm': case 'M': v *= 1024UL * 1024UL; break;
    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;
    default:
        cfg_error(cfg, "%s", "bad suffix '%%s'", val, end);
        return -1;
    }
    cfg->fuzz_resume = (long)v;
    return 0;
}

/* fuzz.verbosity — one of: fast, balanced, safe, aggressive */
static int h_fuzz_verbosity(const char *val, cfg_t *cfg) {
    if (strcmp(val, "fast") == 0) {
        cfg->fuzz_verbosity = 0;
        return 0;
    }
    if (strcmp(val, "balanced") == 0) {
        cfg->fuzz_verbosity = 1;
        return 0;
    }
    if (strcmp(val, "safe") == 0) {
        cfg->fuzz_verbosity = 2;
        return 0;
    }
    if (strcmp(val, "aggressive") == 0) {
        cfg->fuzz_verbosity = 3;
        return 0;
    }
    cfg_error(cfg, "fuzz.verbosity", "unknown option: '%s'", val);
    return -1;
}

/* ---- dispatch table ---- */
static const struct handler handlers[] = {
    {"server.bind_address", h_server_bind_address},
    {"server.bind_port", h_server_bind_port},
    {"server.backlog", h_server_backlog},
    {"server.max_conns", h_server_max_conns},
    {"server.keepalive_timeout", h_server_keepalive_timeout},
    {"server.threads", h_server_threads},
    {"server.worker_affinity", h_server_worker_affinity},
    {"server.max_request_size", h_server_max_request_size},
    {"server.read_timeout", h_server_read_timeout},
    {"server.write_timeout", h_server_write_timeout},
    {"server.graceful_shutdown", h_server_graceful_shutdown},
    {"server.ipv6_only", h_server_ipv6_only},
    {"server.socket_tx_buf", h_server_socket_tx_buf},
    {"server.socket_rx_buf", h_server_socket_rx_buf},
    {"server.http2_h2c", h_server_http2_h2c},
    {"tls.cert_file", h_tls_cert_file},
    {"tls.key_file", h_tls_key_file},
    {"tls.ca_file", h_tls_ca_file},
    {"tls.ciphers", h_tls_ciphers},
    {"tls.min_version", h_tls_min_version},
    {"tls.session_cache", h_tls_session_cache},
    {"tls.tickets", h_tls_tickets},
    {"tls.hsts", h_tls_hsts},
    {"tls.ocsp", h_tls_ocsp},
    {"tls.client_auth", h_tls_client_auth},
    {"tls.early_data", h_tls_early_data},
    {"tls.psk", h_tls_psk},
    {"tls.keylog", h_tls_keylog},
    {"tls.session_timeout", h_tls_session_timeout},
    {"tls.renegotiate", h_tls_renegotiate},
    {"log.level", h_log_level},
    {"log.file", h_log_file},
    {"log.max_size", h_log_max_size},
    {"log.rotate", h_log_rotate},
    {"log.compress", h_log_compress},
    {"log.format", h_log_format},
    {"log.timestamp", h_log_timestamp},
    {"log.pid", h_log_pid},
    {"log.thread_id", h_log_thread_id},
    {"log.line_num", h_log_line_num},
    {"log.color", h_log_color},
    {"log.json", h_log_json},
    {"log.sync", h_log_sync},
    {"log.buffer_kb", h_log_buffer_kb},
    {"log.filter", h_log_filter},
    {"cache.size", h_cache_size},
    {"cache.policy", h_cache_policy},
    {"cache.ttl", h_cache_ttl},
    {"cache.shards", h_cache_shards},
    {"cache.write_through", h_cache_write_through},
    {"cache.max_entry", h_cache_max_entry},
    {"cache.prefetch", h_cache_prefetch},
    {"cache.invalidate", h_cache_invalidate},
    {"cache.rehash", h_cache_rehash},
    {"cache.aligned", h_cache_aligned},
    {"cache.eviction", h_cache_eviction},
    {"cache.lock_stripes", h_cache_lock_stripes},
    {"cache.cold_threshold", h_cache_cold_threshold},
    {"cache.hit_ratio", h_cache_hit_ratio},
    {"cache.bypass", h_cache_bypass},
    {"db.host", h_db_host},
    {"db.port", h_db_port},
    {"db.user", h_db_user},
    {"db.password", h_db_password},
    {"db.database", h_db_database},
    {"db.pool_size", h_db_pool_size},
    {"db.connect_timeout", h_db_connect_timeout},
    {"db.idle_timeout", h_db_idle_timeout},
    {"db.ssl", h_db_ssl},
    {"db.replica", h_db_replica},
    {"db.charset", h_db_charset},
    {"db.max_retries", h_db_max_retries},
    {"db.backoff", h_db_backoff},
    {"db.read_only", h_db_read_only},
    {"db.statement_cache", h_db_statement_cache},
    {"net.listen_backlog", h_net_listen_backlog},
    {"net.nodelay", h_net_nodelay},
    {"net.quickack", h_net_quickack},
    {"net.tcp_keepalive", h_net_tcp_keepalive},
    {"net.max_conn_per_ip", h_net_max_conn_per_ip},
    {"net.send_buffer", h_net_send_buffer},
    {"net.recv_buffer", h_net_recv_buffer},
    {"net.proxy_protocol", h_net_proxy_protocol},
    {"net.reuse_port", h_net_reuse_port},
    {"net.dscp", h_net_dscp},
    {"net.tcp_keepidle", h_net_tcp_keepidle},
    {"net.tcp_keepintvl", h_net_tcp_keepintvl},
    {"net.tcp_keepcnt", h_net_tcp_keepcnt},
    {"net.backlog_accept", h_net_backlog_accept},
    {"net.tun_only", h_net_tun_only},
    {"security.key_rotation", h_security_key_rotation},
    {"security.max_login_attempts", h_security_max_login_attempts},
    {"security.lockout_seconds", h_security_lockout_seconds},
    {"security.password_min_len", h_security_password_min_len},
    {"security.password_min_class", h_security_password_min_class},
    {"security.session_timeout", h_security_session_timeout},
    {"security.token_ttl", h_security_token_ttl},
    {"security.rate_limit", h_security_rate_limit},
    {"security.allow_headers", h_security_allow_headers},
    {"security.deny_ranges", h_security_deny_ranges},
    {"security.csp", h_security_csp},
    {"security.hsts_age", h_security_hsts_age},
    {"security.secure_cookies", h_security_secure_cookies},
    {"security.same_site", h_security_same_site},
    {"security.mfa", h_security_mfa},
    {"mq.uri", h_mq_uri},
    {"mq.reconnect", h_mq_reconnect},
    {"mq.heartbeat", h_mq_heartbeat},
    {"mq.prefetch", h_mq_prefetch},
    {"mq.ack_timeout", h_mq_ack_timeout},
    {"mq.max_redeliver", h_mq_max_redeliver},
    {"mq.dead_letter", h_mq_dead_letter},
    {"mq.max_batch", h_mq_max_batch},
    {"mq.publish_confirm", h_mq_publish_confirm},
    {"mq.consumer_threads", h_mq_consumer_threads},
    {"mq.vhost", h_mq_vhost},
    {"mq.queue_ttl", h_mq_queue_ttl},
    {"mq.flow_control", h_mq_flow_control},
    {"mq.connection_timeout", h_mq_connection_timeout},
    {"mq.tls_verify", h_mq_tls_verify},
    {"metrics.enabled", h_metrics_enabled},
    {"metrics.endpoint", h_metrics_endpoint},
    {"metrics.interval", h_metrics_interval},
    {"metrics.retention", h_metrics_retention},
    {"metrics.aggregate", h_metrics_aggregate},
    {"metrics.labels", h_metrics_labels},
    {"metrics.histogram", h_metrics_histogram},
    {"metrics.samples", h_metrics_samples},
    {"metrics.push_url", h_metrics_push_url},
    {"metrics.token", h_metrics_token},
    {"metrics.scrape_timeout", h_metrics_scrape_timeout},
    {"metrics.chunk_size", h_metrics_chunk_size},
    {"metrics.compression", h_metrics_compression},
    {"metrics.tls", h_metrics_tls},
    {"metrics.tags", h_metrics_tags},
    {"fuzz.seed", h_fuzz_seed},
    {"fuzz.dict", h_fuzz_dict},
    {"fuzz.max_input", h_fuzz_max_input},
    {"fuzz.runs", h_fuzz_runs},
    {"fuzz.workers", h_fuzz_workers},
    {"fuzz.timeout", h_fuzz_timeout},
    {"fuzz.artifact_dir", h_fuzz_artifact_dir},
    {"fuzz.coverage", h_fuzz_coverage},
    {"fuzz.crash_retain", h_fuzz_crash_retain},
    {"fuzz.bisect", h_fuzz_bisect},
    {"fuzz.compare_exit", h_fuzz_compare_exit},
    {"fuzz.dedup", h_fuzz_dedup},
    {"fuzz.queue_depth", h_fuzz_queue_depth},
    {"fuzz.resume", h_fuzz_resume},
    {"fuzz.verbosity", h_fuzz_verbosity},
};

static size_t handler_count(void)
{
    return sizeof(handlers) / sizeof(handlers[0]);
}

static const char *skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return p;
}

static char *rtrim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        e--;
    *e = '\0';
    return s;
}

static int find_handler(const char *key)
{
    size_t lo = 0, hi = handler_count();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int c = strcmp(handlers[mid].key, key);
        if (c == 0)
            return (int)mid;
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return -1;
}

static int cfg_parse_line(cfg_t *cfg, char *line)
{
    char *p = (char *)skip_ws(line);
    if (*p == '\0' || *p == '#' || *p == ';')
        return 0;
    char *eq = strchr(p, '=');
    if (!eq) {
        cfg_error(cfg, line, "missing '='");
        return -1;
    }
    *eq = '\0';
    rtrim(p);
    const char *val = skip_ws(eq + 1);
    if (*val == '\0') {
        cfg_error(cfg, p, "empty value");
        return -1;
    }
    int i = find_handler(p);
    if (i < 0) {
        cfg_error(cfg, p, "unknown key");
        return -1;
    }
    return handlers[i].fn(val, cfg);
}

int cfg_parse_file(cfg_t *cfg, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cfg: cannot open %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    char line[1024];
    int bad = 0;
    while (fgets(line, sizeof line, f)) {
        if (cfg_parse_line(cfg, line) != 0)
            bad++;
    }
    fclose(f);
    return bad ? -1 : 0;
}

static void cfg_error(cfg_t *cfg, const char *key,
                      const char *fmt, ...)
{
    (void)cfg;
    fprintf(stderr, "cfg: key '%s': ", key);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
