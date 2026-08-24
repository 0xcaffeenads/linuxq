/*
 * app.c - Aşırı basit Cartesi "echo" DApp.
 *
 * Ne yapar:
 *  - advance_state (on-chain input) geldiğinde: payload'u aynen bir
 *    "notice" olarak zincire kaydedilecek şekilde geri yollar.
 *  - inspect_state (off-chain sorgu) geldiğinde: payload'u aynen bir
 *    "report" olarak geri yollar.
 *
 * Dış bağımlılık yok (libcurl vs. kullanmaz) - sadece POSIX socket API.
 * ROLLUP_HTTP_SERVER_URL environment değişkeni Cartesi machine tarafından
 * otomatik set edilir (örn: "http://127.0.0.1:5004").
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUF_SZ 65536

static char rollup_host[256];
static int  rollup_port = 5004;

/* ROLLUP_HTTP_SERVER_URL="http://127.0.0.1:5004" -> host + port ayikla */
static void parse_rollup_url(const char *url) {
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *colon = strchr(p, ':');
    if (colon) {
        size_t hl = (size_t)(colon - p);
        if (hl >= sizeof(rollup_host)) hl = sizeof(rollup_host) - 1;
        memcpy(rollup_host, p, hl);
        rollup_host[hl] = '\0';
        rollup_port = atoi(colon + 1);
    } else {
        snprintf(rollup_host, sizeof(rollup_host), "%s", p);
    }
}

static int connect_rollup(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)rollup_port);

    if (inet_pton(AF_INET, rollup_host, &addr.sin_addr) != 1) {
        struct hostent *he = gethostbyname(rollup_host);
        if (!he) { fprintf(stderr, "[app] host cozulemedi: %s\n", rollup_host); exit(1); }
        memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }
    return fd;
}

/* Basit HTTP/1.1 POST, "Connection: close" ile - cevabi tamamen okur.
 * Donen buffer caller tarafindan free edilmeli. status_out'a HTTP kodu yazilir. */
static char *http_post(const char *path, const char *json_body, int *status_out) {
    int fd = connect_rollup();

    size_t body_len = strlen(json_body);
    char req[BUF_SZ];
    int n = snprintf(req, sizeof(req),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, rollup_host, body_len, json_body);

    if (send(fd, req, (size_t)n, 0) < 0) { perror("send"); exit(1); }

    char *resp = malloc(BUF_SZ);
    size_t total = 0;
    ssize_t r;
    while ((r = recv(fd, resp + total, BUF_SZ - 1 - total, 0)) > 0) {
        total += (size_t)r;
        if (total >= BUF_SZ - 1) break;
    }
    resp[total] = '\0';
    close(fd);

    *status_out = 0;
    if (!strncmp(resp, "HTTP/1.1 ", 9) || !strncmp(resp, "HTTP/1.0 ", 9)) {
        *status_out = atoi(resp + 9);
    }
    return resp;
}

static const char *http_body(const char *raw_resp) {
    const char *sep = strstr(raw_resp, "\r\n\r\n");
    return sep ? sep + 4 : "";
}

/* body icinde "\"key\":\"VALUE\"" arar, VALUE'yu statik buffer'a kopyalar. */
static int json_extract_str(const char *body, const char *key, char *out, size_t out_sz) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *p = strstr(body, needle);
    if (!p) return 0;
    p += strlen(needle);
    const char *end = strchr(p, '"');
    if (!end) return 0;
    size_t len = (size_t)(end - p);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

int main(void) {
    const char *url = getenv("ROLLUP_HTTP_SERVER_URL");
    if (!url) {
        fprintf(stderr, "[app] ROLLUP_HTTP_SERVER_URL set degil, cikiliyor\n");
        return 1;
    }
    parse_rollup_url(url);
    printf("[app] echo dapp basladi, rollup server: %s:%d\n", rollup_host, rollup_port);

    char finish_body[64] = "{\"status\":\"accept\"}";

    for (;;) {
        int status = 0;
        char *resp = http_post("/finish", finish_body, &status);
        const char *body = http_body(resp);

        if (status == 202) {
            /* host modunda: bekleyen istek yok, tekrar dene */
            free(resp);
            usleep(200000);
            continue;
        }

        char req_type[32] = {0};
        char payload[BUF_SZ] = {0};
        json_extract_str(body, "request_type", req_type, sizeof(req_type));
        json_extract_str(body, "payload", payload, sizeof(payload));

        if (strcmp(req_type, "advance_state") == 0) {
            printf("[app] advance_state, payload=%s\n", payload);

            char out_body[BUF_SZ + 64];
            snprintf(out_body, sizeof(out_body), "{\"payload\":\"%s\"}", payload);

            int s2 = 0;
            char *nresp = http_post("/notice", out_body, &s2);
            printf("[app] notice gonderildi, status=%d\n", s2);
            free(nresp);

            snprintf(finish_body, sizeof(finish_body), "{\"status\":\"accept\"}");

        } else if (strcmp(req_type, "inspect_state") == 0) {
            printf("[app] inspect_state, payload=%s\n", payload);

            char out_body[BUF_SZ + 64];
            snprintf(out_body, sizeof(out_body), "{\"payload\":\"%s\"}", payload);

            int s2 = 0;
            char *rresp = http_post("/report", out_body, &s2);
            printf("[app] report gonderildi, status=%d\n", s2);
            free(rresp);

            snprintf(finish_body, sizeof(finish_body), "{\"status\":\"accept\"}");
        } else {
            printf("[app] bilinmeyen request_type: '%s'\n", req_type);
        }

        free(resp);
    }

    return 0;
}
