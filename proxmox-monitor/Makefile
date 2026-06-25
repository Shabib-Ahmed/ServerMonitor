# Cross-compile for OpenWRT (set CROSS_COMPILE in your environment,
# e.g. export CROSS_COMPILE=mipsel-openwrt-linux-)
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -Wall -Wextra -Os -std=c99
TARGET  = proxmox-monitor

$(TARGET): proxmox_monitor.c
	$(CC) $(CFLAGS) -o $(TARGET) proxmox_monitor.c

install:
	cp $(TARGET) /usr/bin/$(TARGET)
	cp ../files/etc/init.d/proxmox-monitor /etc/init.d/
	chmod +x /etc/init.d/proxmox-monitor
	/etc/init.d/proxmox-monitor enable

clean:
	rm -f $(TARGET)
