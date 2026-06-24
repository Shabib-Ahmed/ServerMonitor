. /lib/functions.sh

ENABLED=$(uci get monitor.settings.enabled 2>/dev/null)
TARGET_IP=$(uci get monitor.settings.target_ip 2>/dev/null)
TARGET_PORT=$(uci get monitor.settings.target_port 2>/dev/null)

SMTP_HOST=$(uci get monitor.settings.smtp_host 2>/dev/null)
SMTP_PORT=$(uci get monitor.settings.smtp_port 2>/dev/null)
SMTP_USER=$(uci get monitor.settings.smtp_user 2>/dev/null)
SMTP_PASS=$(uci get monitor.settings.smtp_pass 2>/dev/null)
RECIPIENT=$(uci get monitor.settings.alert_recipient 2>/dev/null)

if [ "$ENABLED" != "1" ] || [ -z "$TARGET_IP" ] || [ -z "$SMTP_USER" ]; then
    exit 0
fi

STATE_FILE="/tmp/monitor_server_down"

ping -c 3 -W 1 "$TARGET_IP" > /dev/null 2>&1
L3_STATUS=$?

L7_STATUS=0
if [ -n "$TARGET_PORT" ]; then
    nc -w 2 -z "$TARGET_IP" "$TARGET_PORT" > /dev/null 2>&1
    L7_STATUS=$?
fi

send_email_alert() {
    local subject="$1"
    local body="$2"

    (
        printf "From: monitor <%s>\n" "$SMTP_USER"
        printf "To: %s\n" "$RECIPIENT"
        printf "Subject: %s\n" "$subject"
        printf "Date: %s\n" "$(date -R)"
        printf "MIME-Version: 1.0\n"
        printf "Content-Type: text/plain; charset=utf-8\n\n"
        printf "%s\n" "$body"
    ) | msmtp --host="$SMTP_HOST" \
              --port="$SMTP_PORT" \
              --from="$SMTP_USER" \
              --user="$SMTP_USER" \
              --passwordeval="echo '$SMTP_PASS'" \
              --auth=on \
              --tls=on \
              --tls-starttls=on \
              --tls-certcheck=off \
              "$RECIPIENT" > /dev/null 2>&1 &
}

# State-Transition Evaluation
if [ $L3_STATUS -ne 0 ] || [ $L7_STATUS -ne 0 ]; then
    if [ ! -f "$STATE_FILE" ]; then
        touch "$STATE_FILE"
        send_email_alert "Home Server Offline"
    fi
else
    if [ -f "$STATE_FILE" ]; then
        rm "$STATE_FILE"
        send_email_alert "Home Server Rebooted"
    fi
fi