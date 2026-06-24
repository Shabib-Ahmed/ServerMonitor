# openwrt-server-monitor

Monitoring Package dedicated to monitoring home server on old router with openwrt.

## Deployment
Put the files in their respective folders in your OpenWrt build directory:
```bash
make image PROFILE="your_router" PACKAGES="msmtp" FILES="path/to/files"