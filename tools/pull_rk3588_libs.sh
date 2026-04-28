#!/bin/bash
# =============================================================================
# 从 RK3588 板子拉取平台 .so 库到开发机，用于交叉编译
# -----------------------------------------------------------------------------
# 用法： ./pull_rk3588_libs.sh <板子IP> [用户名]
# 示例： ./pull_rk3588_libs.sh 192.168.1.100 root
# =============================================================================

set -e

BOARD_IP="${1:-}"
USER="${2:-root}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/RK3588_Libs"

if [ -z "$BOARD_IP" ]; then
    echo "Usage: $0 <board_ip> [user]"
    exit 1
fi

echo "==> Target: $USER@$BOARD_IP"
echo "==> Dest:   $DEST"
mkdir -p "$DEST/lib" "$DEST/include"

echo ""
echo "==> Pulling .so files ..."

LIBS=(librockit librockchip_mpp librga libdrm librkaiq libmali \
      libgraphic_lsf librknnrt libRkSwCac librkAlgoDis libasound)

for prefix in "${LIBS[@]}"; do
    echo "    -> $prefix*"
    rsync -avL --no-perms \
        "$USER@$BOARD_IP:/usr/lib/${prefix}*" \
        "$DEST/lib/" 2>/dev/null || echo "       (skipped, not found on board)"
done

echo ""
echo "==> Pulling headers ..."
for d in /usr/include/rockchip /usr/include/rga /usr/include/rknn /usr/include/drm; do
    echo "    -> $d"
    rsync -av "$USER@$BOARD_IP:$d" "$DEST/include/" 2>/dev/null \
        || echo "       (skipped, not found)"
done

rsync -av "$USER@$BOARD_IP:/usr/include/rknn_api.h" "$DEST/include/" 2>/dev/null || true
rsync -av "$USER@$BOARD_IP:/usr/include/rga.h"      "$DEST/include/" 2>/dev/null || true

echo ""
echo "==> Done. Pulled:"
echo "    Libraries:"
ls -1 "$DEST/lib/" | head -30
echo "    Headers:"
ls -1 "$DEST/include/" 2>/dev/null
echo ""
echo "==> Disk usage:"
du -sh "$DEST"
