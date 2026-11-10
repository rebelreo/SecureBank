#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "bank.h"

#define PORT 8080
#define BUFFER_SIZE 16384
#define MAX_SESSIONS 100
#define SESSION_TIMEOUT 1800

typedef struct {
    char id[65];
    int account_no;
    time_t expires_at;
    int active;
} Session;

static Session sessions[MAX_SESSIONS];
typedef struct {
    char id[65];
    time_t expires_at;
    int active;
} AdminSession;

static AdminSession admin_sessions[MAX_SESSIONS];
int create_admin_session(char *session_id, size_t output_size)
{
    unsigned char random_bytes[32];
    const char hex[] = "0123456789abcdef";
    FILE *fp;
    int slot = -1;
    time_t now = time(NULL);

    if (session_id == NULL || output_size < 65)
        return 0;

    fp = fopen("/dev/urandom", "rb");
    if (fp == NULL)
        return 0;

    if (fread(random_bytes, 1, 32, fp) != 32) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (admin_sessions[i].active &&
            admin_sessions[i].expires_at <= now) {
            admin_sessions[i].active = 0;
            admin_sessions[i].id[0] = '\0';
        }

        if (!admin_sessions[i].active && slot == -1)
            slot = i;
    }

    if (slot == -1)
        return 0;

    for (int i = 0; i < 32; i++) {
        session_id[i * 2] = hex[random_bytes[i] >> 4];
        session_id[i * 2 + 1] = hex[random_bytes[i] & 15];
    }

    session_id[64] = '\0';

    strcpy(admin_sessions[slot].id, session_id);
    admin_sessions[slot].expires_at = now + SESSION_TIMEOUT;
    admin_sessions[slot].active = 1;

    return 1;
}

int get_admin_session(const char *session_id)
{
    time_t now = time(NULL);

    if (session_id == NULL || strlen(session_id) != 64)
        return 0;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (admin_sessions[i].active &&
            admin_sessions[i].expires_at > now &&
            strcmp(admin_sessions[i].id, session_id) == 0) {
            admin_sessions[i].expires_at = now + SESSION_TIMEOUT;
            return 1;
        }
    }

    return 0;
}

void destroy_admin_session(const char *session_id)
{
    if (session_id == NULL)
        return;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (admin_sessions[i].active &&
            strcmp(admin_sessions[i].id, session_id) == 0) {
            admin_sessions[i].active = 0;
            admin_sessions[i].id[0] = '\0';
            admin_sessions[i].expires_at = 0;
            return;
        }
    }
}
int create_session(int account_no, char *session_id,
                   size_t output_size) {
    unsigned char random_bytes[32];
    const char hex[] = "0123456789abcdef";
    FILE *random_file;
    int slot = -1;
    time_t now = time(NULL);

    if (account_no <= 0 || session_id == NULL ||
        output_size < 65)
        return 0;

    random_file = fopen("/dev/urandom", "rb");

    if (random_file == NULL)
        return 0;

    if (fread(random_bytes, 1, sizeof(random_bytes),
              random_file) != sizeof(random_bytes)) {
        fclose(random_file);
        return 0;
    }

    fclose(random_file);

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            sessions[i].expires_at <= now) {
            sessions[i].active = 0;
            sessions[i].id[0] = '\0';
            sessions[i].account_no = 0;
            sessions[i].expires_at = 0;
        }

        if (!sessions[i].active && slot == -1)
            slot = i;
    }

    if (slot == -1)
        return 0;

    for (int i = 0; i < 32; i++) {
        session_id[i * 2] = hex[random_bytes[i] >> 4];
        session_id[i * 2 + 1] =
            hex[random_bytes[i] & 15];
    }

    session_id[64] = '\0';

    strcpy(sessions[slot].id, session_id);
    sessions[slot].account_no = account_no;
    sessions[slot].expires_at = now + SESSION_TIMEOUT;
    sessions[slot].active = 1;

    return 1;
}

int get_session_account(const char *session_id) {
    time_t now = time(NULL);

    if (session_id == NULL || strlen(session_id) != 64)
        return 0;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            sessions[i].expires_at > now &&
            strcmp(sessions[i].id, session_id) == 0) {
            sessions[i].expires_at = now + SESSION_TIMEOUT;
            return sessions[i].account_no;
        }

        if (sessions[i].active &&
            sessions[i].expires_at <= now) {
            sessions[i].active = 0;
            sessions[i].id[0] = '\0';
            sessions[i].account_no = 0;
            sessions[i].expires_at = 0;
        }
    }

    return 0;
}

void destroy_session(const char *session_id) {
    if (session_id == NULL)
        return;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            strcmp(sessions[i].id, session_id) == 0) {
            sessions[i].active = 0;
            sessions[i].id[0] = '\0';
            sessions[i].account_no = 0;
            sessions[i].expires_at = 0;
            return;
        }
    }
}

int send_all(int client, const char *data, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(client, data + sent, length - sent, 0);
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

void send_html(int client, int status, const char *message) {
    char body[2048];
    char header[512];

    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<title>SecureBank</title></head>"
        "<body style='font-family:Arial;padding:40px;background:#f4f6fa'>"
        "<h1>SecureBank</h1><h2>%s</h2>"
        "<p><a href='/'>Return to Home</a></p></body></html>",
        message);

    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, status == 200 ? "OK" :
        status == 400 ? "Bad Request" : "Not Found",
        strlen(body));

    send_all(client, header, strlen(header));
    send_all(client, body, strlen(body));
}
void send_html_cookie(int client, int status,
                      const char *message,
                      const char *session_id) {
    char body[2048];
    char header[1024];

    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<title>SecureBank</title></head>"
        "<body style='font-family:Arial;padding:40px;"
        "background:#f4f6fa'>"
        "<h1>SecureBank</h1><h2>%s</h2>"
        "<p><a href='/'>Return to Home</a></p>"
        "</body></html>", message);

    int header_length;

    if (session_id != NULL) {
        header_length = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %zu\r\n"
            "Set-Cookie: SECUREBANK_SESSION=%s; "
            "Path=/; HttpOnly; SameSite=Strict; "
            "Max-Age=1800\r\n"
            "Connection: close\r\n\r\n",
            strlen(body), session_id);
    } else {
        header_length = snprintf(header, sizeof(header),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %zu\r\n"
            "Set-Cookie: SECUREBANK_SESSION=; "
            "Path=/; HttpOnly; SameSite=Strict; "
            "Max-Age=0\r\n"
            "Connection: close\r\n\r\n",
            status,
            status == 401 ? "Unauthorized" : "OK",
            strlen(body));
    }

    if (header_length > 0 &&
        (size_t)header_length < sizeof(header)) {
        send_all(client, header, (size_t)header_length);
        send_all(client, body, strlen(body));
    }
}
void serve_css(int client) {
    FILE *fp = fopen("html/style.css", "rb");

    if (!fp) {
        send_html(client, 404, "CSS file not found");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0 || size > 500000) {
        fclose(fp);
        send_html(client, 500, "Invalid CSS file size");
        return;
    }

    char *css = malloc((size_t)size);

    if (!css) {
        fclose(fp);
        send_html(client, 500, "Memory allocation failed");
        return;
    }

    size_t bytes = fread(css, 1, (size_t)size, fp);
    fclose(fp);

    if (bytes != (size_t)size) {
        free(css);
        send_html(client, 500, "Unable to read CSS");
        return;
    }

    char header[512];

    int length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/css; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        bytes);

    if (length > 0 && (size_t)length < sizeof(header)) {
        send_all(client, header, (size_t)length);
        send_all(client, css, bytes);
    }

    free(css);
}
void serve_logo(int client) {
    FILE *fp = fopen("html/securebank-logo.jpg", "rb");

    if (!fp) {
        send_html(client, 404, "Logo file not found");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size <= 0 || size > 2000000) {
        fclose(fp);
        send_html(client, 500, "Invalid logo file size");
        return;
    }

    char *image = malloc((size_t)size);

    if (!image) {
        fclose(fp);
        send_html(client, 500, "Memory allocation failed");
        return;
    }

    size_t bytes = fread(image, 1, (size_t)size, fp);
    fclose(fp);

    if (bytes != (size_t)size) {
        free(image);
        send_html(client, 500, "Unable to read logo");
        return;
    }

    char header[512];

    int header_length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        bytes);

    if (header_length > 0 &&
        (size_t)header_length < sizeof(header)) {
        send_all(client, header, (size_t)header_length);
        send_all(client, image, bytes);
    }

    free(image);
}
void serve_home(int client) {
    FILE *fp = fopen("html/index.html", "rb");

    if (!fp) {
        send_html(client, 500, "HTML file not available");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0 || size > 1000000) {
        fclose(fp);
        send_html(client, 500, "Invalid HTML file size");
        return;
    }

    char *body = malloc((size_t)size);

    if (!body) {
        fclose(fp);
        send_html(client, 500, "Memory allocation failed");
        return;
    }

    size_t bytes = fread(body, 1, (size_t)size, fp);
    fclose(fp);

    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n", bytes);

    send_all(client, header, strlen(header));
    send_all(client, body, bytes);

    free(body);
}

size_t get_content_length(const char *headers) {
    const char *p = strstr(headers, "\r\n");

    if (!p) return 0;
    p += 2;

    while (*p) {
        const char *end = strstr(p, "\r\n");
        if (!end || end == p) break;

        if ((size_t)(end - p) > 15 &&
            strncasecmp(p, "Content-Length:", 15) == 0) {
            return (size_t)strtoul(p + 15, NULL, 10);
        }

        p = end + 2;
    }
    return 0;
}

void url_decode(char *text) {
    char *src = text;
    char *dst = text;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            char *end;
            long value = strtol(hex, &end, 16);

            if (*end == '\0') {
                *dst++ = (char)value;
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void get_field(char *body, const char *key,
               char *output, size_t output_size) {
    output[0] = '\0';

    char copy[BUFFER_SIZE];
    snprintf(copy, sizeof(copy), "%s", body);

    char *save = NULL;
    char *pair = strtok_r(copy, "&", &save);

    while (pair) {
        char *equal = strchr(pair, '=');

        if (equal) {
            *equal = '\0';
            char *value = equal + 1;

            url_decode(pair);
            url_decode(value);

            if (strcmp(pair, key) == 0) {
                snprintf(output, output_size, "%s", value);
                return;
            }
        }

        pair = strtok_r(NULL, "&", &save);
    }
}
void handle_create(int client, char *body) {
    char name[100] = "";
    char mobile[32] = "";
    char password[64] = "";
    char type[32] = "";
    char balance_text[64] = "";

    get_field(body, "name", name, sizeof(name));
    get_field(body, "mobile", mobile, sizeof(mobile));
    get_field(body, "password", password, sizeof(password));
    get_field(body, "type", type, sizeof(type));
    get_field(body, "opening_balance",
              balance_text, sizeof(balance_text));

    if (!name[0] || !mobile[0] || !password[0] ||
        !type[0] || !balance_text[0]) {
        send_html(client, 400, "Please fill all account details");
        return;
    }

    errno = 0;
    char *end;
    double balance = strtod(balance_text, &end);

    if (errno != 0 || end == balance_text ||
        *end != '\0' || !isfinite(balance) || balance < 0) {
        send_html(client, 400, "Invalid opening deposit");
        return;
    }

    int account_no = 0;

    int result = create_account(
        name, mobile, password, type, balance, &account_no
    );

    if (result == 1) {
        char message[256];

        snprintf(message, sizeof(message),
            "Account Created Successfully! Account Number: %d",
            account_no);

        send_html(client, 200, message);
    } else {
        send_html(client, 400,
            "Account creation failed. Check your details and try again.");
    }
}
void send_dashboard(int client, Account *account,
                    const char *session_id) {
    char name[256];
    char type[128];
    char body[8192];
    char header[1024];

    snprintf(name, sizeof(name), "%s", account->name);
    snprintf(type, sizeof(type), "%s", account->accountType);

    snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>SecureBank Dashboard</title>"
        "<style>"
        "body{margin:0;font-family:Arial;background:#f1f5f9;color:#1e293b}"
        "header{background:#102a43;color:white;padding:22px 8%%}"
        ".container{max-width:950px;margin:40px auto;padding:20px}"
        ".card{background:white;padding:25px;border-radius:12px;"
        "margin:18px 0;box-shadow:0 3px 12px #0001}"
        ".balance{font-size:32px;color:#087f5b;font-weight:bold}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}"
        ".label{color:#64748b;font-size:14px}"
        ".value{font-size:20px;font-weight:bold;margin-top:8px}"
        ".button{display:inline-block;background:#087f5b;color:white;"
        "padding:12px 22px;border-radius:7px;text-decoration:none}"
        "@media(max-width:600px){.grid{grid-template-columns:1fr}}"
        "</style></head><body>"

        "<header><h1>SecureBank</h1>"
        "<p>Customer Dashboard</p></header>"

        "<div class='container'>"
        "<h2>Welcome, %s!</h2>"

               "<div class='card'>"
        "<div class='label'>Available Balance</div>"
        "<div class='balance'>Rs. %.2f</div>"
        "</div>"

        "<div class='grid'>"
        "<div class='card'>"
        "<div class='label'>Account Number</div>"
        "<div class='value'>%d</div>"
        "</div>"

        "<div class='card'>"
        "<div class='label'>Account Type</div>"
        "<div class='value'>%s</div>"
        "</div>"
        "</div>"

        "<div class='grid'>"
        "<div class='card'>"
        "<h3>Deposit Money</h3>"
        "<form action='/deposit' method='POST'>"
        "<label>Amount (Rs.)</label>"
        "<input type='number' name='amount' "
        "min='0.01' step='0.01' required "
        "style='padding:12px;width:90%%;margin:12px 0'>"
        "<button class='button' type='submit'>Deposit</button>"
        "</form>"
        "</div>"

        "<div class='card'>"
        "<h3>Withdraw Money</h3>"
        "<form action='/withdraw' method='POST'>"
        "<label>Amount (Rs.)</label>"
        "<input type='number' name='amount' "
        "min='0.01' step='0.01' required "
        "style='padding:12px;width:90%%;margin:12px 0'>"
        "<button class='button' type='submit'>Withdraw</button>"
        "</form>"
        "</div>"
                "</div>"
       "<div class='card'>"
"<h3>Transfer Money</h3>"
"<form action='/transfer' method='POST'>"

"<label>Receiver Account Number</label>"
"<input type='number' name='receiver_account' "
"min='100001' required "
"style='padding:12px;width:90%%;margin:12px 0'>"

"<label>Transfer Amount (Rs.)</label>"
"<input type='number' name='amount' "
"min='0.01' step='0.01' required "
"style='padding:12px;width:90%%;margin:12px 0'>"

"<button class='button' type='submit'>"
"Transfer Money</button>"

"</form>"
"</div>"
        "<div class='card' style='text-align:center'>"
        "<a class='button' href='/transactions'>"
        "View Transaction History</a>"
        "</div>"
"<div class='card' style='text-align:center'>"
"<a class='button' href='/profile'>"
"My Profile</a>"
"</div>"
        "<div class='card' style='text-align:center'>"
"<form action='/logout' method='POST'>"
"<button class='button' type='submit'>Logout</button>"
"</form>"
"</div>"

        "</div></body></html>",

        name, account->balance,
        account->accountNo, type);

    int length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Set-Cookie: SECUREBANK_SESSION=%s; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=1800\r\n"
        "Connection: close\r\n\r\n",
        strlen(body), session_id);

    if (length > 0 && (size_t)length < sizeof(header)) {
        send_all(client, header, (size_t)length);
        send_all(client, body, strlen(body));
    }
}

int get_session_cookie(const char *request, char *output,
                       size_t output_size) {
    const char *p = request;

    output[0] = '\0';

    while ((p = strstr(p, "\r\n")) != NULL) {
        p += 2;

        if (*p == '\r' || *p == '\0')
            break;

        if (strncasecmp(p, "Cookie:", 7) == 0) {
            const char *end = strstr(p, "\r\n");
            const char *q = p + 7;

            while (q < end) {
                while (q < end && (*q == ' ' || *q == ';'))
                    q++;

                const char *equal = memchr(q, '=', end - q);

                if (!equal)
                    break;

                const char *value = equal + 1;
                const char *semi = memchr(value, ';', end - value);
                const char *value_end = semi ? semi : end;

                while (value_end > value &&
                       value_end[-1] == ' ')
                    value_end--;

                if ((size_t)(equal - q) == 18 &&
                    strncmp(q, "SECUREBANK_SESSION", 18) == 0) {

                    size_t len = (size_t)(value_end - value);

                    if (len != 64 || output_size < 65)
                        return 0;

                    for (size_t i = 0; i < len; i++) {
                        if (!((value[i] >= '0' && value[i] <= '9') ||
                              (value[i] >= 'a' && value[i] <= 'f')))
                            return 0;
                    }

                    memcpy(output, value, len);
                    output[len] = '\0';
                    return 1;
                }

                if (!semi)
                    break;

                q = semi + 1;
            }
        }
    }

    return 0;
}
int get_admin_session_cookie(const char *request,
                             char *output,
                             size_t output_size)
{
    const char *p = request;

    if (request == NULL || output == NULL || output_size < 65)
        return 0;

    output[0] = '\0';

    while ((p = strstr(p, "\r\n")) != NULL) {
        p += 2;

        if (*p == '\r' || *p == '\0')
            break;

        if (strncasecmp(p, "Cookie:", 7) == 0) {
            const char *end = strstr(p, "\r\n");
            const char *q = p + 7;

            while (q < end) {
                while (q < end && (*q == ' ' || *q == ';'))
                    q++;

                const char *equal = memchr(q, '=', end - q);
                if (!equal)
                    break;

                const char *value = equal + 1;
                const char *semi = memchr(value, ';', end - value);
                const char *value_end = semi ? semi : end;

                while (value_end > value && value_end[-1] == ' ')
                    value_end--;

                if ((size_t)(equal - q) == 24 &&
                    strncmp(q, "SECUREBANK_ADMIN_SESSION", 24) == 0) {

                    size_t len = (size_t)(value_end - value);

                    if (len != 64)
                        return 0;

                    for (size_t i = 0; i < len; i++) {
                        if (!((value[i] >= '0' && value[i] <= '9') ||
                              (value[i] >= 'a' && value[i] <= 'f')))
                            return 0;
                    }

                    memcpy(output, value, len);
                    output[len] = '\0';
                    return 1;
                }

                if (!semi)
                    break;

                q = semi + 1;
            }
        }
    }

    return 0;
}
void send_transaction_history(int client, const char *session_id,
                              int account_no)
{
    Transaction transactions[100];
    char body[12000];
    char header[1024];

    int count = get_transactions(account_no, transactions, 100);

    int used = snprintf(body, sizeof(body),
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Transaction History</title>"
        "<style>"
        "body{font-family:Arial;background:#f1f5f9;padding:20px;color:#1e293b}"
        ".card{background:white;padding:20px;border-radius:10px;max-width:950px;margin:25px auto}"
        "table{width:100%%;border-collapse:collapse}"
        "th,td{padding:12px;border-bottom:1px solid #ddd;text-align:left}"
        "th{background:#102a43;color:white}"
        "a{display:inline-block;margin-top:20px;background:#087f5b;color:white;padding:12px 20px;text-decoration:none;border-radius:6px}"
        "</style></head><body>"
        "<div class='card'><h1>Transaction History</h1>"
        "<table><tr><th>ID</th><th>Type</th>"
        "<th>Amount</th><th>Date & Time</th>"
        "<th>Balance After</th></tr>");

    for (int i = count - 1; i >= 0 && used > 0 &&
         (size_t)used < sizeof(body); i--)
    {
        char date[64];
        time_t timestamp = (time_t)transactions[i].timestamp;
        struct tm *tm_info = localtime(&timestamp);

        if (tm_info != NULL)
            strftime(date, sizeof(date), "%d-%m-%Y %I:%M:%S %p", tm_info);
        else
            snprintf(date, sizeof(date), "Unknown");

        int added = snprintf(body + used, sizeof(body) - (size_t)used,
            "<tr><td>%llu</td><td>%s</td>"
            "<td>Rs. %.2f</td><td>%s</td>"
            "<td>Rs. %.2f</td></tr>",
            transactions[i].transaction_id,
            transactions[i].type,
            transactions[i].amount,
            date,
            transactions[i].balance_after);

        if (added < 0 || (size_t)added >= sizeof(body) - (size_t)used)
            break;

        used += added;
    }

    if (count == 0 && used > 0 && (size_t)used < sizeof(body))
        used += snprintf(body + used, sizeof(body) - (size_t)used,
            "<tr><td colspan='5'>No transactions found.</td></tr>");

    if (used > 0 && (size_t)used < sizeof(body))
        used += snprintf(body + used, sizeof(body) - (size_t)used,
            "</table><a href='/dashboard'>Back to Dashboard</a>"
            "</div></body></html>");

    int header_length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Cache-Control: no-store\r\n"
        "Set-Cookie: SECUREBANK_SESSION=%s; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=1800\r\n"
        "Connection: close\r\n\r\n",
        used, session_id);

    if (header_length > 0 &&
        (size_t)header_length < sizeof(header) &&
        used > 0 && (size_t)used < sizeof(body))
    {
        send_all(client, header, (size_t)header_length);
        send_all(client, body, (size_t)used);
    }
}
void send_admin_dashboard(int client)
{
    Account accounts[500];
    int count = get_all_accounts(accounts, 500);

    if (count < 0) {
        send_html(client, 500, "Unable to load accounts");
        return;
    }

    char page[65536];
    int used = snprintf(page, sizeof(page),
        "<!DOCTYPE html><html><head>"
        "<title>SecureBank Admin Dashboard</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "</head><body>"
        "<h1>SecureBank Admin Dashboard</h1>"
        "<p>Active accounts: %d</p>"
        "<table border='1' cellpadding='8'>"
        "<tr><th>Account No.</th><th>Name</th>"
        "<th>Mobile</th><th>Account Type</th><th>Balance</th></tr>",
        count);

    if (used < 0 || (size_t)used >= sizeof(page)) {
        send_html(client, 500, "Dashboard generation failed");
        return;
    }

    for (int i = 0; i < count; i++) {
        int added = snprintf(page + used, sizeof(page) - (size_t)used,
            "<tr><td>%d</td><td>%s</td><td>%s</td>"
            "<td>%s</td><td>Rs. %.2f</td></tr>",
            accounts[i].accountNo,
            accounts[i].name,
            accounts[i].mobile,
            accounts[i].accountType,
            accounts[i].balance);

        if (added < 0 || (size_t)added >= sizeof(page) - (size_t)used) {
            send_html(client, 500, "Dashboard is too large");
            return;
        }

        used += added;
    }

    int added = snprintf(page + used, sizeof(page) - (size_t)used,
        "</table><p><a href='/admin-logout'>Logout</a></p>"
        "</body></html>");

    if (added < 0 || (size_t)added >= sizeof(page) - (size_t)used) {
        send_html(client, 500, "Dashboard generation failed");
        return;
    }

    used += added;

    send_html(client, 200, page);
}
void handle_transaction(int client, const char *request,
                        char *body, int is_deposit) {

    char session_id[65];
    char amount_text[64] = "";
    Account account;
    double new_balance;

    /* Verify customer session */
    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int account_no = get_session_account(session_id);

    if (account_no <= 0 ||
        !get_account_details(account_no, &account)) {
        send_html(client, 401, "Session expired. Please login again.");
        return;
    }

    /* Read transaction amount */
    get_field(body, "amount", amount_text,
              sizeof(amount_text));

    if (amount_text[0] == '\0') {
        send_html(client, 400, "Please enter an amount");
        return;
    }

    errno = 0;
    char *end;
    double amount = strtod(amount_text, &end);

    if (errno != 0 || end == amount_text ||
        *end != '\0' || !isfinite(amount) || amount <= 0) {
        send_html(client, 400, "Invalid transaction amount");
        return;
    }

    /* Process transaction */
    int result;

    if (is_deposit) {
        result = deposit_money(account_no, amount,
                               &new_balance);
    } else {
        if (amount > account.balance) {
            send_html(client, 400, "Insufficient account balance");
            return;
        }

        result = withdraw_money(account_no, amount,
                                &new_balance);
    }

    if (!result) {
        send_html(client, 400,
                  "Transaction failed. Please try again.");
        return;
    }

    /* Retrieve updated account details */
    if (!get_account_details(account_no, &account)) {
        send_html(client, 500, "Unable to retrieve updated balance");
        return;
    }

    send_dashboard(client, &account, session_id);
}
void handle_transfer(int client, const char *request, char *body)
{
    char session_id[65];
    char receiver_text[32] = "";
    char amount_text[64] = "";

    char *end;
    long receiver_account;
    double amount;

    double sender_balance;
    double receiver_balance;

    Account sender;

    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int sender_account = get_session_account(session_id);

    if (sender_account <= 0 ||
        !get_account_details(sender_account, &sender)) {
        send_html(client, 401, "Session expired");
        return;
    }

    get_field(body, "receiver_account",
              receiver_text, sizeof(receiver_text));

    get_field(body, "amount",
              amount_text, sizeof(amount_text));

    errno = 0;
    receiver_account = strtol(receiver_text, &end, 10);

    if (errno != 0 || end == receiver_text ||
        *end != '\0' || receiver_account <= 0 ||
        receiver_account > 2147483647L) {
        send_html(client, 400, "Invalid receiver account");
        return;
    }

    errno = 0;
    amount = strtod(amount_text, &end);

    if (errno != 0 || end == amount_text ||
        *end != '\0' || !isfinite(amount) || amount <= 0) {
        send_html(client, 400, "Invalid transfer amount");
        return;
    }

    if (sender_account == (int)receiver_account) {
        send_html(client, 400,
                  "You cannot transfer money to yourself");
        return;
    }

    if (!transfer_money(
            sender_account,
            (int)receiver_account,
            amount,
            &sender_balance,
            &receiver_balance)) {

        send_html(client, 400,
                  "Transfer failed. Check receiver account and balance.");
        return;
    }

    if (!get_account_details(sender_account, &sender)) {
        send_html(client, 500,
                  "Transfer completed but dashboard refresh failed");
        return;
    }

    send_dashboard(client, &sender, session_id);
}
void handle_login(int client, char *body) {
    char account_text[32] = "";
    char password[MAX_PASS] = "";

    get_field(body, "account_no",
              account_text, sizeof(account_text));

    get_field(body, "password",
              password, sizeof(password));

    if (account_text[0] == '\0' || password[0] == '\0') {
        send_html(client, 400,
                  "Please enter account number and password");
        return;
    }

    char *end;
    errno = 0;

    long account_no = strtol(account_text, &end, 10);

    if (errno != 0 || end == account_text ||
        *end != '\0' || account_no <= 0 ||
        account_no > 2147483647L) {
        send_html(client, 400, "Invalid account number");
        return;
    }

    Account account;

    if (authenticate_account((int)account_no,
                             password, &account) != 1) {
        send_html(client, 401,
                  "Invalid account number or password");
        return;
    }

    char session_id[65];

    if (!create_session((int)account_no,
                        session_id, sizeof(session_id))) {
        send_html(client, 500,
                  "Unable to create session");
        return;
    }

    char message[512];

    snprintf(message, sizeof(message),
             "Login Successful! Welcome to SecureBank. "
             "Account Number: %d | Balance: Rs. %.2f",
             account.accountNo, account.balance);

    send_dashboard(client, &account, session_id);
}
void handle_admin_login(int client, char *body)
{
    char username[64] = "";
    char password[128] = "";

    const char *admin_user = getenv("SECUREBANK_ADMIN_USER");
    const char *admin_pass = getenv("SECUREBANK_ADMIN_PASS");

    get_field(body, "username", username, sizeof(username));
    get_field(body, "password", password, sizeof(password));

    if (admin_user == NULL || admin_pass == NULL ||
        admin_user[0] == '\0' || admin_pass[0] == '\0') {
        send_html(client, 500, "Admin credentials are not configured");
        return;
    }

    if (strcmp(username, admin_user) != 0 ||
        strcmp(password, admin_pass) != 0) {
        send_html(client, 401, "Invalid admin username or password");
        return;
    }

    char session_id[65];

    if (!create_admin_session(session_id, sizeof(session_id))) {
        send_html(client, 500, "Unable to create admin session");
        return;
    }

    const char *response =
        "<!DOCTYPE html><html><body>"
        "<h1>Admin authentication successful</h1>"
        "<p>Admin session created successfully.</p>"
        "<a href='/admin-dashboard'>Continue to Admin Dashboard</a>"
        "</body></html>";

    char header[1024];

    int length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Set-Cookie: SECUREBANK_ADMIN_SESSION=%s; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=1800\r\n"
        "Connection: close\r\n\r\n",
        strlen(response), session_id);

    if (length > 0 && (size_t)length < sizeof(header)) {
        send_all(client, header, (size_t)length);
        send_all(client, response, strlen(response));
    }
}
void send_profile_page(int client, Account *account,
                       const char *session_id)
{
    char body[6000];
    char header[1024];

    snprintf(body, sizeof(body),
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>My Profile - SecureBank</title>"
        "<style>"
        "body{font-family:Arial;background:#f1f5f9;margin:0;color:#1e293b}"
        ".container{max-width:650px;margin:40px auto;padding:20px}"
        ".card{background:white;padding:25px;border-radius:12px;"
        "box-shadow:0 3px 12px #0001}"
        "input{width:95%%;padding:12px;margin:10px 0;"
        "border:1px solid #ccc;border-radius:6px}"
        "button,a{background:#087f5b;color:white;padding:12px 20px;"
        "border:0;border-radius:6px;text-decoration:none;cursor:pointer}"
        "</style></head><body>"
        "<div class='container'>"
        "<div class='card'>"
        "<h1>My Profile</h1>"
        "<p><b>Account Number:</b> %d</p>"
        "<p><b>Name:</b> %s</p>"
        "<p><b>Mobile:</b> %s</p>"
        "<p><b>Account Type:</b> %s</p>"
        "<p><b>Balance:</b> Rs. %.2f</p>"
        "<hr>"
        "<h3>Update Profile</h3>"
        "<form action='/update-profile' method='POST'>"
        "<label>New Name</label>"
        "<input type='text' name='name' maxlength='49' required>"
        "<label>New Mobile</label>"
        "<input type='tel' name='mobile' maxlength='10' "
        "pattern='[0-9]{10}' required>"
        "<button type='submit'>Update Profile</button>"
        "</form>"
         "<hr>"
"<h3>Change Password</h3>"
"<form action='/change-password' method='POST'>"
"<label>Current Password</label>"
"<input type='password' name='old_password' "
"maxlength='31' required>"
"<label>New Password</label>"
"<input type='password' name='new_password' "
"minlength='4' maxlength='31' required>"
"<button type='submit'>Change Password</button>"
"</form>"
        "<br><a href='/dashboard'>Back to Dashboard</a>"
        "</div></div></body></html>",
        account->accountNo,
        account->name,
        account->mobile,
        account->accountType,
        account->balance);

    int length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Set-Cookie: SECUREBANK_SESSION=%s; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=1800\r\n"
        "Connection: close\r\n\r\n",
        strlen(body), session_id);

    if (length > 0 && (size_t)length < sizeof(header))
    {
        send_all(client, header, (size_t)length);
        send_all(client, body, strlen(body));
    }
}
void handle_profile_update(int client, const char *request,
                           char *body)
{
    char session_id[65];
    char name[MAX_NAME] = "";
    char mobile[MAX_MOBILE] = "";
    Account account;

    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int account_no = get_session_account(session_id);

    if (account_no <= 0 ||
        !get_account_details(account_no, &account)) {
        send_html(client, 401, "Session expired");
        return;
    }

    get_field(body, "name", name, sizeof(name));
    get_field(body, "mobile", mobile, sizeof(mobile));

    if (name[0] == '\0' || mobile[0] == '\0') {
        send_html(client, 400, "Please fill all profile details");
        return;
    }

    if (!update_customer_profile(account_no, name, mobile)) {
    send_html(client, 400, "Unable to update profile");
    return;
    }

    if (!get_account_details(account_no, &account)) {
        send_html(client, 500, "Unable to retrieve profile");
        return;
    }

    send_dashboard(client, &account, session_id);
}
void handle_password_change(int client, const char *request,
                            char *body)
{
    char session_id[65];
    char old_password[MAX_PASS] = "";
    char new_password[MAX_PASS] = "";

    Account account;

    /* Verify customer session */
    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int account_no = get_session_account(session_id);

    if (account_no <= 0 ||
        !get_account_details(account_no, &account)) {
        send_html(client, 401, "Session expired");
        return;
    }

    /* Read passwords */
    get_field(body, "old_password",
              old_password, sizeof(old_password));

    get_field(body, "new_password",
              new_password, sizeof(new_password));

    if (old_password[0] == '\0' ||
        new_password[0] == '\0') {
        send_html(client, 400, "Please fill both password fields");
        return;
    }

    if (strlen(new_password) < 4 ||
        strlen(new_password) >= MAX_PASS) {
        send_html(client, 400, "New password must contain 4-31 characters");
        return;
    }

    if (strcmp(old_password, new_password) == 0) {
        send_html(client, 400, "New password must be different");
        return;
    }

    /* Change password */
    if (!change_customer_password(account_no,
                                  old_password,
                                  new_password)) {
        send_html(client, 400,
                  "Incorrect current password or update failed");
        return;
    }

    destroy_session(session_id);

send_html(client, 200,
          "Password changed successfully. Please login again.");
}
void handle_client(int client) {
    char request[BUFFER_SIZE];
    size_t used = 0;
    size_t header_end = 0;
    size_t content_length = 0;

    memset(request, 0, sizeof(request));

    while (used < sizeof(request) - 1) {
        ssize_t n = recv(client, request + used,
                         sizeof(request) - 1 - used, 0);

        if (n <= 0) return;

        used += (size_t)n;
        request[used] = '\0';

        char *separator = strstr(request, "\r\n\r\n");

        if (separator) {
            header_end = (size_t)(separator - request) + 4;
            content_length = get_content_length(request);

            if (content_length > sizeof(request) - header_end - 1) {
                send_html(client, 400, "Request too large");
                return;
            }

            if (used >= header_end + content_length)
                break;
        }
    }

    if (!header_end || used < header_end + content_length) {
        send_html(client, 400, "Incomplete HTTP request");
        return;
    }

    char method[16] = "";
    char path[256] = "";

    if (sscanf(request, "%15s %255s", method, path) != 2) {
        send_html(client, 400, "Invalid request");
        return;
    }
    if (strcmp(method, "GET") == 0 &&
    strcmp(path, "/securebank-logo.jpg") == 0) {
    serve_logo(client);
    return;
}
  if (strcmp(method, "GET") == 0 &&
    strcmp(path, "/style.css") == 0) {
    serve_css(client);
    return;
}
    if (strcmp(method, "GET") == 0 &&
        (strcmp(path, "/") == 0 ||
         strcmp(path, "/index.html") == 0)) {
        serve_home(client);
        return;
    }

    if (strcmp(method, "POST") == 0 &&
        strcmp(path, "/create") == 0) {
        char *body = request + header_end;
        body[content_length] = '\0';
        handle_create(client, body);
        return;
    }
if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/admin-login") == 0) {

    char *body = request + header_end;
    body[content_length] = '\0';

    handle_admin_login(client, body);
    return;
}
if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/login") == 0) {

    char *body = request + header_end;
    body[content_length] = '\0';

    handle_login(client, body);
    return;
}
if (strcmp(method, "POST") == 0 &&
    (strcmp(path, "/deposit") == 0 ||
     strcmp(path, "/withdraw") == 0)) {

    char *body = request + header_end;
    body[content_length] = '\0';

    int is_deposit = (strcmp(path, "/deposit") == 0);

    handle_transaction(client, request, body, is_deposit);
    return;
}
if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/transfer") == 0) {

    char *body = request + header_end;
    body[content_length] = '\0';

    handle_transfer(client, request, body);
    return;
}
if (strcmp(method, "GET") == 0 &&
    strcmp(path, "/admin-dashboard") == 0) {

    char admin_session_id[65];

    if (!get_admin_session_cookie(request,
                                  admin_session_id,
                                  sizeof(admin_session_id))) {
        send_html(client, 401, "Admin login required");
        return;
    }

    if (!get_admin_session(admin_session_id)) {
        send_html(client, 401, "Admin session expired");
        return;
    }

    send_admin_dashboard(client);
    return;
}
if (strcmp(method, "GET") == 0 &&
    (strcmp(path, "/dashboard") == 0 ||
     strcmp(path, "/profile") == 0)) {

    char session_id[65];
    Account account;

    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int account_no = get_session_account(session_id);

    if (account_no <= 0 ||
        !get_account_details(account_no, &account)) {
        send_html(client, 401,
                  "Session expired. Please login again.");
        return;
    }

    if (strcmp(path, "/profile") == 0) {
        send_profile_page(client, &account, session_id);
    } else {
        send_dashboard(client, &account, session_id);
    }

    return;
}
if (strcmp(method, "GET") == 0 &&
    strcmp(path, "/transactions") == 0) {
    char session_id[65];

    if (!get_session_cookie(request, session_id,
                            sizeof(session_id))) {
        send_html(client, 401, "Please login first");
        return;
    }

    int account_no = get_session_account(session_id);

    if (account_no <= 0) {
        send_html(client, 401, "Session expired. Please login again.");
        return;
    }

    send_transaction_history(client, session_id, account_no);
    return;
}
if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/update-profile") == 0) {

    char *body = request + header_end;
    body[content_length] = '\0';

    handle_profile_update(client, request, body);
    return;
}
if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/change-password") == 0) {

    char *body = request + header_end;
    body[content_length] = '\0';

    handle_password_change(client, request, body);
    return;
}
if (strcmp(method, "GET") == 0 &&
    strcmp(path, "/admin-logout") == 0) {

    char admin_session_id[65];

    if (get_admin_session_cookie(request,
                                 admin_session_id,
                                 sizeof(admin_session_id))) {
        destroy_admin_session(admin_session_id);
    }

    const char *response =
        "<!DOCTYPE html><html><body>"
        "<h2>Admin logged out successfully</h2>"
        "<a href='/'>Return to Home</a>"
        "</body></html>";

    char header[512];

    int header_length = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Set-Cookie: SECUREBANK_ADMIN_SESSION=; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=0\r\n"
        "Connection: close\r\n\r\n",
        strlen(response));

    if (header_length > 0 &&
        (size_t)header_length < sizeof(header)) {
        send_all(client, header, (size_t)header_length);
        send_all(client, response, strlen(response));
    }

    return;
}

if (strcmp(method, "POST") == 0 &&
    strcmp(path, "/logout") == 0) {

    char session_id[65];

    if (get_session_cookie(request, session_id,
                           sizeof(session_id))) {
        destroy_session(session_id);
    }

    send_html(client, 200, "You have been logged out successfully.");
    return;
}
    send_html(client, 404, "Page or service not found");
}

int main(void) {
    int server_fd, client;
    int option = 1;

    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket failed");
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &option, sizeof(option));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    printf("SecureBank C Server started!\n");
    printf("Open http://127.0.0.1:%d\n", PORT);

    while (1) {
        client = accept(server_fd, NULL, NULL);

        if (client < 0) {
            if (errno == EINTR) continue;
            perror("Accept failed");
            continue;
        }

        handle_client(client);
        close(client);
    }

    close(server_fd);
    return 0;
}
