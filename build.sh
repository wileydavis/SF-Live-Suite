#!/usr/bin/env bash
# SF Live Suite — one-line build: ./build.sh
# Builds every plugin as AU + VST3 (Release) and installs them into
#   ~/Library/Audio/Plug-Ins/Components   (AU)
#   ~/Library/Audio/Plug-Ins/VST3         (VST3)
# Then rescan plug-ins in Live (Settings > Plug-Ins > Rescan).
# Build one plugin only:  ./build.sh KICK
set -euo pipefail
cd "$(dirname "$0")"

JUCE_TAG="8.0.4"
ONLY="${1:-}"

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode command line tools missing: run  xcode-select --install"; exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    echo "Installing CMake via Homebrew..."; brew install cmake
  else
    echo "CMake missing and Homebrew not found. Install Homebrew (https://brew.sh) then re-run."; exit 1
  fi
fi
if [ ! -f JUCE/CMakeLists.txt ]; then
  echo "Cloning JUCE $JUCE_TAG..."
  git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git JUCE
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release -DSF_ONLY="$ONLY" -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)"

# Make macOS forget old AUs so Live sees the new builds.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo
echo "Installed:"
ls -d ~/Library/Audio/Plug-Ins/Components/SF\ *.component 2>/dev/null | sed 's|.*/||' || true
echo
echo "Then in Live: Settings > Plug-Ins > Rescan."
