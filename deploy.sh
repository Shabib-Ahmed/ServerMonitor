set -e

ROUTER="${1:?Usage: ./deploy.sh user@router-ip}"

echo "Deploying to $ROUTER..."

ssh "$ROUTER" "[ -f /etc/config/monitor ]" 2>/dev/null \
    && echo "Skipping config: /etc/config/monitor already exists on router" \
    || scp files/etc/config/monitor "$ROUTER:/etc/config/monitor"

scp files/usr/bin/monitor.sh "$ROUTER:/usr/bin/monitor.sh"
ssh "$ROUTER" "chmod +x /usr/bin/monitor.sh"

scp files/etc/crontabs/root "$ROUTER:/etc/crontabs/root"

if [ ! -f proxmox-monitor/proxmox-monitor ]; then
    echo "Error: proxmox-monitor binary not found. Run 'make' in proxmox-monitor/ first."
    exit 1
fi
scp proxmox-monitor/proxmox-monitor "$ROUTER:/usr/bin/proxmox-monitor"

scp files/etc/init.d/proxmox-monitor "$ROUTER:/etc/init.d/proxmox-monitor"
ssh "$ROUTER" "chmod +x /etc/init.d/proxmox-monitor"

ssh "$ROUTER" "/etc/init.d/proxmox-monitor enable && /etc/init.d/proxmox-monitor start"

echo "Done."
