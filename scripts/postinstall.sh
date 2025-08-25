#!/bin/bash
# Post-install script for PWAR

# Update desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications
fi

# Update icon cache
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -t /usr/share/icons/hicolor
fi

# Update library cache
if command -v ldconfig >/dev/null 2>&1; then
    ldconfig
fi

echo "PWAR has been installed successfully!"
echo "Run 'pwar_cli --help' for CLI usage or 'pwar_gui' for the graphical interface."
