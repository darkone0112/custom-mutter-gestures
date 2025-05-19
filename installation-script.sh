#!/bin/bash
set -e

echo ""
echo "🔧 Mutter Patch Installation Script"

# Step 4: Install generated .deb packages
echo ""
echo "[4/8] 📦 Installing new Mutter .deb packages..."
sudo dpkg -i ../*.deb
echo "[4/8] ✅ Packages installed."

# Step 5: Extract the .so from built package
echo ""
echo "[5/8] 📂 Extracting new libmutter shared object..."
mkdir -p /tmp/mutter-patch-extract
dpkg-deb -x ../libmutter-14-0_*.deb /tmp/mutter-patch-extract
echo "[5/8] ✅ Extraction complete."

# Step 6: Replace Mutter runtime library
echo ""
echo "[6/8] 📝 Preparing to replace runtime Mutter library in /lib..."

LIB_PATH="/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0"

# Check if the file has the immutable flag set and remove it temporarily
if lsattr "$LIB_PATH" | grep -q '\-i\-'; then
  echo "[6/8] ⚠️ File is write-protected with chattr +i. Removing protection temporarily..."
  sudo chattr -i "$LIB_PATH"
fi

# Overwrite the library with the new one
sudo cp /tmp/mutter-patch-extract/usr/lib/x86_64-linux-gnu/libmutter-14.so.0.0.0 "$LIB_PATH"
sudo touch "$LIB_PATH"
echo "[6/8] ✅ Runtime library replaced."


# Step 7: Protect against package updates
echo ""
echo "[7/8] 🔒 Marking packages to prevent update and locking .so file..."
sudo apt-mark hold mutter libmutter-14-0 mutter-common mutter-common-bin gir1.2-mutter-14 libmutter-test-14
sudo chattr +i /lib/x86_64-linux-gnu/libmutter-14.so.0.0.0
echo "[7/8] ✅ Packages held and lib locked with chattr."

echo ""
echo "[7/8] 🦊 Patching Firefox to support touchscreen scroll..."

# Make sure the user applications folder exists
mkdir -p ~/.local/share/applications

# Copy and patch Firefox .desktop file only if it hasn't been already
if ! grep -q MOZ_USE_XINPUT2 ~/.local/share/applications/firefox.desktop 2>/dev/null; then
    cp /usr/share/applications/firefox.desktop ~/.local/share/applications/

    # Update all Exec entries that launch Firefox
    sed -i 's|Exec=firefox|Exec=env MOZ_USE_XINPUT2=1 firefox|g' ~/.local/share/applications/firefox.desktop

    # Update desktop database so the change is recognized
    update-desktop-database ~/.local/share/applications

    echo "[7/8] ✅ Firefox launcher patched and desktop database updated."
else
    echo "[7/8] ⚠️  Firefox launcher already patched. Skipping."
fi

# Step 8: Final reboot with countdown
echo ""
echo "[8/8] ✅ Mutter patch fully applied and system secured."
echo ""
echo "🔁 System will reboot in 10 seconds to apply changes. Press Ctrl+C to cancel."
for i in {10..1}; do
  echo -ne "$i...\r"
  sleep 1
done

echo ""
echo "Rebooting now..."
sudo reboot