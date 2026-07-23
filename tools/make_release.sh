#!/usr/bin/env bash
# Package a completed Super Mario World Windows (MinGW) release build on Linux.
# Mirror of tools/make_release.ps1 for x86_64-w64-mingw32 cross builds.
#
# Does NOT build. Build first, then:
#   bash tools/make_release.sh -Version 0.9.0 -Variant coop \
#     -BuildDir build-mingw \
#     -RuntimeBinDir /usr/x86_64-w64-mingw32/bin
set -euo pipefail

Version=""
Variant="stock"
BuildDir="build-mingw"
RuntimeBinDir="/usr/x86_64-w64-mingw32/bin"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -Version) Version="${2:?}"; shift 2 ;;
    -Variant) Variant="${2:?}"; shift 2 ;;
    -BuildDir) BuildDir="${2:?}"; shift 2 ;;
    -RuntimeBinDir) RuntimeBinDir="${2:?}"; shift 2 ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

if [[ -z "$Version" ]]; then
  echo "Usage: $0 -Version X.Y.Z [-Variant stock|coop] [-BuildDir DIR] [-RuntimeBinDir DIR]" >&2
  exit 1
fi
if [[ "$Variant" != "stock" && "$Variant" != "coop" ]]; then
  echo "Variant must be stock or coop" >&2
  exit 1
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/$BuildDir"
if [[ "$Variant" == "coop" ]]; then
  exeName="SuperMarioWorldCoopSNESRecomp.exe"
  stageBase="SuperMarioWorldCoopSNESRecomp"
else
  exeName="SuperMarioWorldSNESRecomp.exe"
  stageBase="SuperMarioWorldRecomp"
fi
exe="$build/$exeName"
assets="$build/assets"

[[ -f "$exe" ]] || { echo "Release executable missing: $exe" >&2; exit 1; }
[[ -d "$assets" ]] || { echo "recomp-ui launcher assets/ missing: $assets" >&2; exit 1; }

out="$root/release-stage"
stageName="${stageBase}-windows-x64-v${Version}"
stage="$out/$stageName"
zip="$out/${stageName}.zip"

rm -rf "$stage" "$zip"
mkdir -p "$stage"

cp -f "$exe" "$stage/"
if [[ "$Variant" == "coop" ]]; then
  cp -f "$root/recomp/coop/smw_coop.ips" "$stage/"
fi
cp -f "$root/README.md" "$stage/"
cp -a "$assets" "$stage/"

kb="$build/keybinds.ini"
[[ -f "$kb" ]] && cp -f "$kb" "$stage/"

sed -E 's/^Widescreen[[:space:]]*=.*/Widescreen = 0/' "$root/config.ini" \
  > "$stage/config.ini"

runtimeDlls=(
  SDL2.dll
  libgcc_s_seh-1.dll
  libstdc++-6.dll
  libwinpthread-1.dll
  libssp-0.dll
)
for name in "${runtimeDlls[@]}"; do
  src="$RuntimeBinDir/$name"
  # Prefer DLL already staged next to the exe by recomp_ui.cmake POST_BUILD.
  if [[ -f "$build/$name" ]]; then
    src="$build/$name"
  elif [[ ! -f "$src" ]]; then
    echo "Required MinGW runtime DLL missing: $name (looked in $build and $RuntimeBinDir)" >&2
    exit 1
  fi
  cp -f "$src" "$stage/"
done

mkdir -p "$out"
( cd "$stage" && zip -qr "$zip" . )
echo "--- $stageName ---"
ls -la "$stage"
sha256sum "$zip"
