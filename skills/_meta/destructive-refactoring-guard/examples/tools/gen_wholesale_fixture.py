#!/usr/bin/env python3
"""Deterministic generator for examples/bad/wholesale_rewrite fixtures.

Writes:
  examples/bad/parser_before.c   a ~2000-line, per-key validating config
                                 parser (the working code being destroyed)
  examples/bad/parser_after.c    a ~100-line "modernized" rewrite that drops
                                 all per-key validation (the replacement)
  examples/bad/wholesale_rewrite.diff   unified diff, path src/parser.c

The two .c files share no lines, so the unified diff is a single full-file
hunk: deleted ~ original size, added ~ rewrite size. Re-running this script
on any host with python 3.11 reproduces byte-identical fixtures.
"""
import difflib
import os

HERE = os.path.dirname(os.path.abspath(__file__))
BAD = os.path.normpath(os.path.join(HERE, "..", "bad"))

SECTIONS = {
    "server": ["bind_address", "bind_port", "backlog", "max_conns",
               "keepalive_timeout", "threads", "worker_affinity",
               "max_request_size", "read_timeout", "write_timeout",
               "graceful_shutdown", "ipv6_only", "socket_tx_buf",
               "socket_rx_buf", "http2_h2c"],
    "tls": ["cert_file", "key_file", "ca_file", "ciphers", "min_version",
            "session_cache", "tickets", "hsts", "ocsp", "client_auth",
            "early_data", "psk", "keylog", "session_timeout", "renegotiate"],
    "log": ["level", "file", "max_size", "rotate", "compress", "format",
            "timestamp", "pid", "thread_id", "line_num", "color", "json",
            "sync", "buffer_kb", "filter"],
    "cache": ["size", "policy", "ttl", "shards", "write_through",
              "max_entry", "prefetch", "invalidate", "rehash", "aligned",
              "eviction", "lock_stripes", "cold_threshold", "hit_ratio",
              "bypass"],
    "db": ["host", "port", "user", "password", "database", "pool_size",
           "connect_timeout", "idle_timeout", "ssl", "replica", "charset",
           "max_retries", "backoff", "read_only", "statement_cache"],
    "net": ["listen_backlog", "nodelay", "quickack", "tcp_keepalive",
            "max_conn_per_ip", "send_buffer", "recv_buffer",
            "proxy_protocol", "reuse_port", "dscp", "tcp_keepidle",
            "tcp_keepintvl", "tcp_keepcnt", "backlog_accept", "tun_only"],
    "security": ["key_rotation", "max_login_attempts", "lockout_seconds",
                 "password_min_len", "password_min_class", "session_timeout",
                 "token_ttl", "rate_limit", "allow_headers", "deny_ranges",
                 "csp", "hsts_age", "secure_cookies", "same_site", "mfa"],
    "mq": ["uri", "reconnect", "heartbeat", "prefetch", "ack_timeout",
           "max_redeliver", "dead_letter", "max_batch", "publish_confirm",
           "consumer_threads", "vhost", "queue_ttl", "flow_control",
           "connection_timeout", "tls_verify"],
    "metrics": ["enabled", "endpoint", "interval", "retention", "aggregate",
                "labels", "histogram", "samples", "push_url", "token",
                "scrape_timeout", "chunk_size", "compression", "tls", "tags"],
    "fuzz": ["seed", "dict", "max_input", "runs", "workers", "timeout",
             "artifact_dir", "coverage", "crash_retain", "bisect",
             "compare_exit", "dedup", "queue_depth", "resume", "verbosity"],
}

# rotate templates deterministically so the generated parser looks like
# hand-written variety: int / bool / string / size-suffix / enum
TYPES = ["int", "bool", "string", "size", "enum"]


def handler_lines(sec, key, ktype, idx):
    full = "%s.%s" % (sec, key)
    fn = "h_%s_%s" % (sec, key)
    field = "cfg->%s_%s" % (sec, key)
    if ktype == "int":
        lo, hi = 1, 65535
        if idx % 3 == 0:
            lo, hi = 0, 1000000
        return [
            "/* %s — integer value in [%d, %d] */" % (full, lo, hi),
            "static int %s(const char *val, cfg_t *cfg) {" % fn,
            "    char *end = NULL;",
            "    long v = strtol(val, &end, 10);",
            "    if (end == val || *end != '\\0') {",
            "        cfg_error(cfg, \"%s\", \"not an integer: '%%s'\", val);" % full,
            "        return -1;",
            "    }",
            "    if (v < %d || v > %d) {" % (lo, hi),
            "        cfg_error(cfg, \"%s\", \"out of range %%d..%%d: %%ld\", %d, %d, v);" % (full, lo, hi),
            "        return -1;",
            "    }",
            "    %s = v;" % field,
            "    return 0;",
            "}",
        ]
    if ktype == "bool":
        return [
            "/* %s — boolean (1/0, true/false, yes/no, on/off) */" % full,
            "static int %s(const char *val, cfg_t *cfg) {" % fn,
            "    if (strcmp(val, \"1\") == 0 || strcmp(val, \"true\") == 0 ||",
            "        strcmp(val, \"yes\") == 0 || strcmp(val, \"on\") == 0) {",
            "        %s = 1;" % field,
            "        return 0;",
            "    }",
            "    if (strcmp(val, \"0\") == 0 || strcmp(val, \"false\") == 0 ||",
            "        strcmp(val, \"no\") == 0 || strcmp(val, \"off\") == 0) {",
            "        %s = 0;" % field,
            "        return 0;",
            "    }",
            "    cfg_error(cfg, \"%s\", \"not a boolean: '%%s'\", val);" % full,
            "    return -1;",
            "}",
        ]
    if ktype == "string":
        return [
            "/* %s — non-empty string, no '=', max 255 bytes */" % full,
            "static int %s(const char *val, cfg_t *cfg) {" % fn,
            "    size_t n = strlen(val);",
            "    if (n == 0 || n > 255) {",
            "        cfg_error(cfg, \"%s\", \"length %%zu out of 1..255\", n);" % full,
            "        return -1;",
            "    }",
            "    if (strchr(val, '=') != NULL) {",
            "        cfg_error(cfg, \"%s\", \"value contains '='\", val);" % full,
            "        return -1;",
            "    }",
            "    snprintf(%s, sizeof(%s), \"%%s\", val);" % (field, field),
            "    return 0;",
            "}",
        ]
    if ktype == "size":
        return [
            "/* %s — size with optional K/M/G suffix */" % full,
            "static int %s(const char *val, cfg_t *cfg) {" % fn,
            "    char *end = NULL;",
            "    unsigned long v = strtoul(val, &end, 10);",
            "    if (end == val || *end == '\\0') {",
            "        cfg_error(cfg, \"%s\", \"missing size suffix: '%%s'\", val);" % full,
            "        return -1;",
            "    }",
            "    switch (*end) {",
            "    case 'k': case 'K': v *= 1024UL; break;",
            "    case 'm': case 'M': v *= 1024UL * 1024UL; break;",
            "    case 'g': case 'G': v *= 1024UL * 1024UL * 1024UL; break;",
            "    default:",
            "        cfg_error(cfg, \"%s\", \"bad suffix '%%s'\", val, end);",
            "        return -1;",
            "    }",
            "    %s = (long)v;" % field,
            "    return 0;",
            "}",
        ]
    # enum
    opts = ["fast", "balanced", "safe", "aggressive"]
    lines = [
        "/* %s — one of: %s */" % (full, ", ".join(opts)),
        "static int %s(const char *val, cfg_t *cfg) {" % fn,
    ]
    for k, opt in enumerate(opts):
        lines.append("    if (strcmp(val, \"%s\") == 0) {" % opt)
        lines.append("        %s = %d;" % (field, k))
        lines.append("        return 0;")
        lines.append("    }")
    lines.append("    cfg_error(cfg, \"%s\", \"unknown option: '%%s'\", val);" % full)
    lines.append("    return -1;")
    lines.append("}")
    return lines


def build_before():
    lines = []
    lines.append("/*")
    lines.append(" * src/parser.c — INI-style configuration parser, v9.4")
    lines.append(" *")
    lines.append(" * 150 per-key validators accumulated over 6 years.")
    lines.append(" * Each key range-checks, type-checks and error-reports its")
    lines.append(" * value; unknown keys are rejected with a message.")
    lines.append(" */")
    lines.append("#include <stdio.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <string.h>")
    lines.append("#include <ctype.h>")
    lines.append("#include <errno.h>")
    lines.append("#include <stdarg.h>")
    lines.append("")
    lines.append("typedef struct cfg_t cfg_t;")
    lines.append("")
    lines.append("struct cfg_t {")
    idx = 0
    for sec, keys in SECTIONS.items():
        for key in keys:
            ktype = TYPES[idx % len(TYPES)]
            idx += 1
            if ktype == "string":
                lines.append("    char %s_%s[256];" % (sec, key))
            else:
                lines.append("    long %s_%s;" % (sec, key))
    lines.append("};")
    lines.append("")
    lines.append("static void cfg_error(cfg_t *cfg, const char *key,")
    lines.append("                      const char *fmt, ...);")
    lines.append("")
    lines.append("struct handler {")
    lines.append("    const char *key;")
    lines.append("    int (*fn)(const char *val, cfg_t *cfg);")
    lines.append("};")
    lines.append("")
    idx = 0
    for sec, keys in SECTIONS.items():
        lines.append("/* ---- section: %s ---- */" % sec)
        lines.append("")
        for key in keys:
            ktype = TYPES[idx % len(TYPES)]
            lines.extend(handler_lines(sec, key, ktype, idx))
            lines.append("")
            idx += 1
    lines.append("/* ---- dispatch table ---- */")
    lines.append("static const struct handler handlers[] = {")
    for sec, keys in SECTIONS.items():
        for key in keys:
            lines.append("    {\"%s.%s\", h_%s_%s}," % (sec, key, sec, key))
    lines.append("};")
    lines.append("")
    lines.append("static size_t handler_count(void)")
    lines.append("{")
    lines.append("    return sizeof(handlers) / sizeof(handlers[0]);")
    lines.append("}")
    lines.append("")
    lines.append("static const char *skip_ws(const char *p)")
    lines.append("{")
    lines.append("    while (*p && isspace((unsigned char)*p))")
    lines.append("        p++;")
    lines.append("    return p;")
    lines.append("}")
    lines.append("")
    lines.append("static char *rtrim(char *s)")
    lines.append("{")
    lines.append("    char *e = s + strlen(s);")
    lines.append("    while (e > s && isspace((unsigned char)e[-1]))")
    lines.append("        e--;")
    lines.append("    *e = '\\0';")
    lines.append("    return s;")
    lines.append("}")
    lines.append("")
    lines.append("static int find_handler(const char *key)")
    lines.append("{")
    lines.append("    size_t lo = 0, hi = handler_count();")
    lines.append("    while (lo < hi) {")
    lines.append("        size_t mid = (lo + hi) / 2;")
    lines.append("        int c = strcmp(handlers[mid].key, key);")
    lines.append("        if (c == 0)")
    lines.append("            return (int)mid;")
    lines.append("        if (c < 0)")
    lines.append("            lo = mid + 1;")
    lines.append("        else")
    lines.append("            hi = mid;")
    lines.append("    }")
    lines.append("    return -1;")
    lines.append("}")
    lines.append("")
    lines.append("static int cfg_parse_line(cfg_t *cfg, char *line)")
    lines.append("{")
    lines.append("    char *p = (char *)skip_ws(line);")
    lines.append("    if (*p == '\\0' || *p == '#' || *p == ';')")
    lines.append("        return 0;")
    lines.append("    char *eq = strchr(p, '=');")
    lines.append("    if (!eq) {")
    lines.append("        cfg_error(cfg, line, \"missing '='\");")
    lines.append("        return -1;")
    lines.append("    }")
    lines.append("    *eq = '\\0';")
    lines.append("    rtrim(p);")
    lines.append("    const char *val = skip_ws(eq + 1);")
    lines.append("    if (*val == '\\0') {")
    lines.append("        cfg_error(cfg, p, \"empty value\");")
    lines.append("        return -1;")
    lines.append("    }")
    lines.append("    int i = find_handler(p);")
    lines.append("    if (i < 0) {")
    lines.append("        cfg_error(cfg, p, \"unknown key\");")
    lines.append("        return -1;")
    lines.append("    }")
    lines.append("    return handlers[i].fn(val, cfg);")
    lines.append("}")
    lines.append("")
    lines.append("int cfg_parse_file(cfg_t *cfg, const char *path)")
    lines.append("{")
    lines.append("    FILE *f = fopen(path, \"r\");")
    lines.append("    if (!f) {")
    lines.append("        fprintf(stderr, \"cfg: cannot open %s: %s\\n\",")
    lines.append("                path, strerror(errno));")
    lines.append("        return -1;")
    lines.append("    }")
    lines.append("    char line[1024];")
    lines.append("    int bad = 0;")
    lines.append("    while (fgets(line, sizeof line, f)) {")
    lines.append("        if (cfg_parse_line(cfg, line) != 0)")
    lines.append("            bad++;")
    lines.append("    }")
    lines.append("    fclose(f);")
    lines.append("    return bad ? -1 : 0;")
    lines.append("}")
    lines.append("")
    lines.append("static void cfg_error(cfg_t *cfg, const char *key,")
    lines.append("                      const char *fmt, ...)")
    lines.append("{")
    lines.append("    (void)cfg;")
    lines.append("    fprintf(stderr, \"cfg: key '%s': \", key);")
    lines.append("    va_list ap;")
    lines.append("    va_start(ap, fmt);")
    lines.append("    vfprintf(stderr, fmt, ap);")
    lines.append("    va_end(ap);")
    lines.append("    fprintf(stderr, \"\\n\");")
    lines.append("}")
    return lines


def build_after():
    lines = []
    lines.append("/*")
    lines.append(" * src/parser.c — configuration parser, v2.0 (rewrite)")
    lines.append(" *")
    lines.append(" * Modernized: the 150 per-key validators were replaced by a")
    lines.append(" * generic key=value splitter. Per-key range/enum/boolean")
    lines.append(" * checks, error messages and unknown-key rejection are gone.")
    lines.append(" * Unknown keys are silently ignored.")
    lines.append(" */")
    lines.append("#include <stdio.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <string.h>")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    char key[64];")
    lines.append("    char val[256];")
    lines.append("} cfg_item;")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    cfg_item items[64];")
    lines.append("    size_t n;")
    lines.append("} cfg_t;")
    lines.append("")
    lines.append("static int cfg_set(cfg_t *cfg, const char *key, const char *val)")
    lines.append("{")
    lines.append("    for (size_t i = 0; i < cfg->n; i++) {")
    lines.append("        if (strcmp(cfg->items[i].key, key) == 0) {")
    lines.append("            snprintf(cfg->items[i].val, sizeof(cfg->items[i].val),")
    lines.append("                     \"%.255s\", val);")
    lines.append("            return 0;")
    lines.append("        }")
    lines.append("    }")
    lines.append("    if (cfg->n >= 64)")
    lines.append("        return -1;")
    lines.append("    cfg_item *it = &cfg->items[cfg->n++];")
    lines.append("    snprintf(it->key, sizeof(it->key), \"%.63s\", key);")
    lines.append("    snprintf(it->val, sizeof(it->val), \"%.255s\", val);")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    lines.append("int cfg_parse_file(cfg_t *cfg, const char *path)")
    lines.append("{")
    lines.append("    FILE *f = fopen(path, \"r\");")
    lines.append("    if (!f)")
    lines.append("        return -1;")
    lines.append("    char line[1024];")
    lines.append("    while (fgets(line, sizeof line, f)) {")
    lines.append("        char *eq = strchr(line, '=');")
    lines.append("        if (!eq)")
    lines.append("            continue;")
    lines.append("        *eq = '\\0';")
    lines.append("        cfg_set(cfg, line, eq + 1);")
    lines.append("    }")
    lines.append("    fclose(f);")
    lines.append("    return 0;")
    lines.append("}")
    return lines


def main():
    before = build_before()
    after = build_after()
    before_text = "\n".join(before) + "\n"
    after_text = "\n".join(after) + "\n"

    p_before = os.path.join(BAD, "parser_before.c")
    p_after = os.path.join(BAD, "parser_after.c")
    with open(p_before, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(before_text)
    with open(p_after, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(after_text)

    diff_lines = difflib.unified_diff(
        before_text.splitlines(), after_text.splitlines(),
        fromfile="src/parser.c", tofile="src/parser.c", lineterm="")
    diff_text = "\n".join(diff_lines) + "\n"
    p_diff = os.path.join(BAD, "wholesale_rewrite.diff")
    with open(p_diff, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(diff_text)

    print("parser_before.c: %d lines" % len(before))
    print("parser_after.c:  %d lines" % len(after))
    print("wholesale_rewrite.diff: %d lines" % len(diff_text.splitlines()))


if __name__ == "__main__":
    main()
