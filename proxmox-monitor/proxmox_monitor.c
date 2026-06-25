#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static int uci_get(const char *key, char *out, size_t len)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "uci get %s 2>/dev/null", key);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    int ok = (fgets(out, len, f) != NULL);
    pclose(f);
    if (ok) {
        size_t n = strlen(out);
        if (n > 0 && out[n-1] == '\n') out[n-1] = '\0';
    }
    return ok ? 0 : -1;
}

typedef struct {
    char enabled[4];
    char proxmox_ip[64];
    char proxmox_port[8];
    char proxmox_node[64];
    char api_token_id[128];
    char api_token_secret[128];
    char temp_threshold[8];     /* degrees Celsius, default 80 */
    char poll_interval[8];      /* seconds, default 60 */
    char smtp_host[128];
    char smtp_port[8];
    char smtp_user[128];
    char smtp_pass[128];
    char alert_recipient[128];
} Config;

static int load_config(Config *c)
{
    uci_get("monitor.settings.enabled",            c->enabled,            sizeof(c->enabled));
    uci_get("monitor.settings.target_ip",          c->proxmox_ip,         sizeof(c->proxmox_ip));
    uci_get("monitor.settings.target_port",        c->proxmox_port,       sizeof(c->proxmox_port));
    uci_get("monitor.settings.proxmox_node",       c->proxmox_node,       sizeof(c->proxmox_node));
    uci_get("monitor.settings.api_token_id",       c->api_token_id,       sizeof(c->api_token_id));
    uci_get("monitor.settings.api_token_secret",   c->api_token_secret,   sizeof(c->api_token_secret));
    uci_get("monitor.settings.temp_threshold",     c->temp_threshold,     sizeof(c->temp_threshold));
    uci_get("monitor.settings.poll_interval",      c->poll_interval,      sizeof(c->poll_interval));
    uci_get("monitor.settings.smtp_host",          c->smtp_host,          sizeof(c->smtp_host));
    uci_get("monitor.settings.smtp_port",          c->smtp_port,          sizeof(c->smtp_port));
    uci_get("monitor.settings.smtp_user",          c->smtp_user,          sizeof(c->smtp_user));
    uci_get("monitor.settings.smtp_pass",          c->smtp_pass,          sizeof(c->smtp_pass));
    uci_get("monitor.settings.alert_recipient",    c->alert_recipient,    sizeof(c->alert_recipient));

    /* defaults */
    if (c->proxmox_port[0] == '\0') strncpy(c->proxmox_port, "8006", sizeof(c->proxmox_port));
    if (c->proxmox_node[0] == '\0') strncpy(c->proxmox_node, "pve",  sizeof(c->proxmox_node));
    if (c->temp_threshold[0] == '\0') strncpy(c->temp_threshold, "80", sizeof(c->temp_threshold));
    if (c->poll_interval[0] == '\0')  strncpy(c->poll_interval,  "60", sizeof(c->poll_interval));

    if (strcmp(c->enabled, "1") != 0)      return -1;
    if (c->proxmox_ip[0] == '\0')          return -1;
    if (c->api_token_id[0] == '\0')        return -1;
    if (c->api_token_secret[0] == '\0')    return -1;
    if (c->smtp_user[0] == '\0')           return -1;
    return 0;
}

static int http_get(const char *host, int port,
                    const char *path, const char *auth_header,
                    char *resp, size_t resp_len)
{
    struct sockaddr_in sa;
    struct hostent *he = gethostbyname(host);
    if (!he) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    memcpy(&sa.sin_addr, he->h_addr, he->h_length);

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s:%d\r\n"
             "Authorization: PVEAPIToken=%s\r\n"
             "Accept: application/json\r\n"
             "\r\n",
             path, host, port, auth_header);

    if (send(fd, req, strlen(req), 0) < 0) { close(fd); return -1; }

    size_t total = 0;
    ssize_t n;
    while (total < resp_len - 1 &&
           (n = recv(fd, resp + total, resp_len - 1 - total, 0)) > 0)
        total += n;
    resp[total] = '\0';
    close(fd);
    return (total > 0) ? 0 : -1;
}


static int json_get_double(const char *json, const char *key, double *out)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    char *end;
    *out = strtod(p, &end);
    return (end != p) ? 0 : -1;
}


static void send_alert(const Config *c, const char *subject, const char *body)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "(printf 'From: monitor <%s>\\nTo: %s\\nSubject: %s\\n"
             "MIME-Version: 1.0\\nContent-Type: text/plain; charset=utf-8\\n\\n%s\\n')"
             " | msmtp --host='%s' --port='%s' --from='%s' --user='%s'"
             " --passwordeval=\"echo '%s'\" --auth=on --tls=on"
             " --tls-starttls=on --tls-certcheck=off '%s' &",
             c->smtp_user, c->alert_recipient, subject, body,
             c->smtp_host, c->smtp_port, c->smtp_user, c->smtp_user,
             c->smtp_pass, c->alert_recipient);
    system(cmd);
}

static volatile int running = 1;
static void handle_sig(int s) { (void)s; running = 0; }

int main(void)
{
    signal(SIGTERM, handle_sig);
    signal(SIGINT,  handle_sig);

    Config cfg;
    if (load_config(&cfg) != 0) {
        fprintf(stderr, "proxmox-monitor: missing or disabled config, exiting\n");
        return 1;
    }

    int   port          = atoi(cfg.proxmox_port);
    int   interval      = atoi(cfg.poll_interval);
    double temp_limit   = atof(cfg.temp_threshold);

    double last_uptime  = -1.0;   
    int    temp_alerted = 0;

    /* Build auth string: token_id=secret */
    char auth[256];
    snprintf(auth, sizeof(auth), "%s=%s", cfg.api_token_id, cfg.api_token_secret);

    char path[256];
    snprintf(path, sizeof(path), "/api2/json/nodes/%s/status", cfg.proxmox_node);

    char resp[8192];

    while (running) {
        memset(resp, 0, sizeof(resp));

        if (http_get(cfg.proxmox_ip, port, path, auth, resp, sizeof(resp)) != 0) {
            last_uptime  = -1.0;
            temp_alerted = 0;
            sleep(interval);
            continue;
        }

        char *body = strstr(resp, "\r\n\r\n");
        if (!body) { sleep(interval); continue; }
        body += 4;

        double uptime = 0.0;
        if (json_get_double(body, "uptime", &uptime) == 0) {
            if (last_uptime > 0.0 && uptime < last_uptime) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Proxmox node '%s' rebooted silently.\n"
                         "Previous uptime: %.0f s  Current uptime: %.0f s",
                         cfg.proxmox_node, last_uptime, uptime);
                send_alert(&cfg, "Home Server Silent Reboot Detected", msg);
            }
            last_uptime = uptime;
        }

        double temp = 0.0;
        if (json_get_double(body, "temp", &temp) == 0) {
            if (temp >= temp_limit && !temp_alerted) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Proxmox node '%s' temperature is %.1f°C (threshold: %.0f°C).",
                         cfg.proxmox_node, temp, temp_limit);
                send_alert(&cfg, "Home Server High Temperature", msg);
                temp_alerted = 1;
            } else if (temp < temp_limit && temp_alerted) {
                temp_alerted = 0; 
            }
        }

        sleep(interval);
    }

    return 0;
}
