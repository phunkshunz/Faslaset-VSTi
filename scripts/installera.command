#!/bin/bash
# Installerar FASLÅSET. Kör från Terminal:  bash installera.command
set -e
cd "$(dirname "$0")"

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

rm -rf "$VST3/FASLASET.vst3" "$AU/FASLASET.component"
cp -R FASLASET.vst3 "$VST3/"
cp -R FASLASET.component "$AU/"

# Pluginet är osignerat, så macOS spärrar det. Detta tar bort spärren.
xattr -dr com.apple.quarantine "$VST3/FASLASET.vst3" "$AU/FASLASET.component" FASLASET.app 2>/dev/null || true

echo ""
echo "Klart! FASLÅSET ligger nu i:"
echo "  $VST3"
echo "Stäng Bitwig helt och starta igen. FASLASET finns bland instrumenten."
echo "Lägg den på ett instrumentspår och tryck play. Den följer tempot själv."
