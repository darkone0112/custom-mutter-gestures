#!/bin/bash
set -e

clear

# ASCII Banner
echo ""
echo "=========================================="
echo "         N E W L I N E   M U T T E R     "
echo "=========================================="
echo ""

# Step 1: Clean previous .deb files
echo "\n[1/8] Cleaning old .deb packages..."
rm -f ../*.deb 2>/dev/null || true
rm -f *.deb 2>/dev/null || true
echo "[1/8] ✅ Old .deb files cleaned (if any existed)."

# Step 2: Commit local patch
echo "\n[2/8] Committing local source changes as a patch..."
cd mutter-46.2
yes | dpkg-source --commit || true
echo "[2/8] ✅ Patch committed to debian/patches/."

# Step 3: Build from source
echo "\n[3/8] Building Mutter packages with dpkg-buildpackage..."
dpkg-buildpackage -us -uc -j$(nproc)
echo "[3/8] ✅ Build complete."

cd ..

# Step 4: Install .deb packages
echo "\n[4/8] Installing new Mutter .deb packages..."
sudo dpkg -i ./*.deb || true
echo "[4/8] ✅ Packages installed (ignore harmless version match errors)."

# Step 5: Extract libmutter
echo "\n[5/8] Extracting libmutter shared object..."
mkdir -p /tmp/mutter-patch-extract
dpkg-deb -x ./libmutter-14-0_*.deb /tmp/mutter-patch-extract
echo "[5/8] ✅ Extraction complete."

# Step 6: Replace runtime lib
LIB_PATH="/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0"
echo "\n[6/8] Replacing runtime Mutter library..."
if lsattr "$LIB_PATH" 2>/dev/null | grep -q '\-i\-'; then
  echo "[6/8] ⚠️ File is locked with chattr +i. Removing protection..."
  sudo chattr -i "$LIB_PATH"
fi
sudo cp /tmp/mutter-patch-extract/usr/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0 "$LIB_PATH"
sudo touch "$LIB_PATH"
echo "[6/8] ✅ Runtime library replaced."

# Step 7: Lock packages and patch Firefox

echo "\n[7/8] Locking packages and enabling touchscreen scroll in Firefox..."
sudo apt-mark hold mutter libmutter-14-0 mutter-common mutter-common-bin gir1.2-mutter-14 libmutter-test-14
sudo chattr +i "$LIB_PATH"

mkdir -p ~/.local/share/applications
if ! grep -q MOZ_USE_XINPUT2 ~/.local/share/applications/firefox.desktop 2>/dev/null; then
  cp /usr/share/applications/firefox.desktop ~/.local/share/applications/
  sed -i 's|Exec=firefox|Exec=env MOZ_USE_XINPUT2=1 firefox|g' ~/.local/share/applications/firefox.desktop
  update-desktop-database ~/.local/share/applications
  echo "[7/8] ✅ Firefox patched for touchscreen scroll."
else
  echo "[7/8] ⚠️ Firefox launcher already patched. Skipping."
fi

echo ""
echo "=========================================="
echo "         N E W L I N E   M U T T E R     "
echo "=========================================="
echo ""

# Step 8: Reboot with countdown
echo "\n[8/8] ✅ Patch applied. System will reboot in 10 seconds. Press Ctrl+C to cancel."
for i in {10..1}; do
  echo -ne "$i...\r"
  sleep 1
done

echo "\nRebooting now..."
sudo reboot