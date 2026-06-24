# ServerMonitor

Multiple scripts that monitor a home server and sends email alerts on state changes, (Temperature and Silent Reboots being worked on).

Written to run on an old repurposed router with minimal dependencies.

## How it works

Every minute, cron runs `monitor.sh`, which performs two checks:

- **L3 (ICMP):** `ping` to verify the host is reachable on the network
- **L7 (TCP):** `nc` port check to verify the service is actually responding

Alerts only fire on state transitions. A `/tmp` state file tracks whether the server was previously seen as down, so you get one email when it goes offline and one when it comes back.

Email is sent via `msmtp` (Gmail SMTP or any STARTTLS provider).

## Requirements

- OpenWRT router with `msmtp` package
- Gmail account (or SMTP credentials for another provider)

## Installation

1. Build your firmware image with the `msmtp` package and include the files:

   ```sh
   make image PROFILE="your_router" PACKAGES="msmtp" FILES="path/to/files"
   ```

2. Edit `/etc/config/monitor` with your settings:

   ```
   config settings 'settings'
       option enabled          '1'
       option target_ip        '192.168.1.50'  # IP of server to monitor
       option target_port      '8006'          # Service port (e.g. Proxmox UI)

       option smtp_host        'smtp.gmail.com'
       option smtp_port        '587'
       option smtp_user        'you@gmail.com'
       option smtp_pass        'your-app-password'

       option alert_recipient  'alerts@example.com'
   ```

3. The cron job runs automatically every minute via `/etc/crontabs/root`.

## Configuration

| Option | Description |
|--------|-------------|
| `enabled` | Set to `1` to activate monitoring |
| `target_ip` | IP address of the server to monitor |
| `target_port` | TCP port for L7 service check (optional) |
| `smtp_*` | SMTP credentials for alert emails |
| `alert_recipient` | Where to send alerts |
