#!/bin/bash
set -e

echo ""
echo "=========================================="
echo "         N E W L I N E   M U T T E R     "
echo "=========================================="
echo ""


LIB_PATH="/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0"

# Step 1: Remove write protection if already present
echo ""
echo "[1/7] 🔓 Checking if libmutter is protected..."
if [ -f "$LIB_PATH" ]; then
  if lsattr "$LIB_PATH" | grep -q '\-i\-'; then
    echo "[1/7] ⚠️ File is protected with chattr +i. Removing protection..."
    sudo chattr -i "$LIB_PATH"
  else
    echo "[1/7] ✅ File is not write-protected."
  fi
else
  echo "[1/7] ℹ️ File does not exist yet. Skipping chattr removal."
fi

# Step 2: Clean up any previous extract directory
echo ""
echo "[2/7] 🧹 Cleaning previous extract directory..."
sudo rm -rf /tmp/mutter-patch-extract
mkdir -p /tmp/mutter-patch-extract
echo "[2/7] ✅ Clean done."

# Step 3: Install all .deb packages
echo ""
echo "[3/7] 📦 Installing Mutter .deb packages..."
sudo dpkg -i ./*.deb || true
echo "[3/7] ✅ Package installation complete (some harmless errors may occur if versions match)."

# Step 4: Extract new .so and copy to runtime path
echo ""
echo "[4/7] 📂 Extracting and replacing libmutter..."
dpkg-deb -x ./libmutter-14-0_*.deb /tmp/mutter-patch-extract
sudo cp /tmp/mutter-patch-extract/usr/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0 "$LIB_PATH"
sudo touch "$LIB_PATH"
echo "[4/7] ✅ libmutter replaced in runtime."

# Step 5: Lock packages and write-protect lib
echo ""
echo "[5/7] 🔒 Locking packages and protecting libmutter..."
sudo apt-mark hold mutter libmutter-14-0 mutter-common mutter-common-bin gir1.2-mutter-14 libmutter-test-14
sudo chattr +i "$LIB_PATH"
echo "[5/7] ✅ Held packages and protected libmutter with chattr."

# Step 6: Patch Firefox for scroll gesture support
echo ""
echo "[6/7] 🦊 Patching Firefox to support touchscreen scroll..."

mkdir -p ~/.local/share/applications

FIREFOX_DESKTOP_SYSTEM="/usr/share/applications/firefox.desktop"
FIREFOX_DESKTOP_USER="$HOME/.local/share/applications/firefox.desktop"

if [ -f "$FIREFOX_DESKTOP_SYSTEM" ]; then
    cp "$FIREFOX_DESKTOP_SYSTEM" "$FIREFOX_DESKTOP_USER"
    sed -i 's|Exec=firefox|Exec=env MOZ_USE_XINPUT2=1 firefox|g' "$FIREFOX_DESKTOP_USER"
    update-desktop-database ~/.local/share/applications
    echo "[6/7] ✅ Firefox .desktop launcher patched (deb version)."
else
    # Snap version fallback
    FIREFOX_SNAP_DESKTOP="/var/lib/snapd/desktop/applications/firefox_firefox.desktop"
    if [ -f "$FIREFOX_SNAP_DESKTOP" ]; then
        cp "$FIREFOX_SNAP_DESKTOP" "$FIREFOX_DESKTOP_USER"
        sed -i 's|Exec=firefox|Exec=env MOZ_USE_XINPUT2=1 firefox|g' "$FIREFOX_DESKTOP_USER"
        update-desktop-database ~/.local/share/applications
        echo "[6/7] ✅ Firefox .desktop launcher patched (Snap version)."
    else
        echo "[6/7] ⚠️ Firefox .desktop file not found in standard or Snap locations. Skipping patch."
    fi
fi

# Step 7: Final reboot
echo ""
echo "=========================================="
echo "         N E W L I N E   M U T T E R     "
echo "=========================================="
echo ""

echo ""
echo "[7/7] ✅ Mutter patch deployed successfully."
echo ""
echo "🔁 System will reboot in 10 seconds. Press Ctrl+C to cancel."
for i in {10..1}; do
  echo -ne "$i...\r"
  sleep 1
done

echo ""
echo "Rebooting now..."
sudo reboot

