#!/bin/bash
set -e

echo ""
echo "=========================================="
echo "         N E W L I N E   M U T T E R     "
echo "=========================================="
echo ""

LIB_PATH="/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0"

echo ""
echo "[1/5] 🔓 Removing chattr protection if present (optional)..."
if [ -f "$LIB_PATH" ]; then
  if lsattr "$LIB_PATH" | grep -q '\-i\-'; then
    chattr -i "$LIB_PATH"
  else
    echo "[1/5] ✅ File is not write-protected."
  fi
else
  echo "[1/5] ℹ️ File does not exist yet. Skipping chattr removal."
fi

echo ""
echo "[2/5] 🧹 Cleaning previous extract directory..."
rm -rf /tmp/mutter-patch-extract
mkdir -p /tmp/mutter-patch-extract

echo ""
echo "[3/5] 📦 Installing Mutter .deb packages..."
dpkg -i ./*.deb || true
apt install -f -y

echo ""
echo "[4/5] 📂 Forcing .so copy to runtime location..."
dpkg-deb -x ./libmutter-14-0_*.deb /tmp/mutter-patch-extract
cp /tmp/mutter-patch-extract/usr/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0 "$LIB_PATH"
touch "$LIB_PATH"

echo ""
echo "[5/5] 🔒 Holding packages..."
apt-mark hold mutter libmutter-14-0 mutter-common mutter-common-bin gir1.2-mutter-14 libmutter-test-14

echo ""
echo "=========================================="
echo "✅ Mutter patch deployed successfully in ISO chroot"
echo "=========================================="
