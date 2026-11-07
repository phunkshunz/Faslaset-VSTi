#!/bin/bash
# Installerar MOTSTAND (VST3 + AU). Kör från Terminal:  bash installera.command
# Installs MOTSTAND (VST3 + AU). Run from Terminal:  bash installera.command
set -e
cd "$(dirname "$0")"

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

rm -rf "$VST3/MOTSTAND.vst3" "$AU/MOTSTAND.component"
cp -R MOTSTAND.vst3 "$VST3/"
cp -R MOTSTAND.component "$AU/"

# Pluginet är osignerat, så macOS spärrar det. Detta tar bort spärren.
# The plugin is unsigned, so macOS blocks it. This removes the block.
xattr -dr com.apple.quarantine "$VST3/MOTSTAND.vst3" "$AU/MOTSTAND.component" MOTSTAND.app 2>/dev/null || true

echo ""
echo "Klart! / Done! MOTSTAND:"
echo "  $VST3"
echo "  $AU"
echo "Starta om din DAW, eller sök efter nya plugins (Bitwig: Settings > Locations > Plug-ins > Rescan)."
echo "Restart your DAW, or rescan plugins (Bitwig: Settings > Locations > Plug-ins > Rescan)."
