#!/bin/zsh
# Minimal Boot device verify — Boot Dashboard B-1…B-8 (+ B-4.x probes).
# First missing B-n = only allowed fix. No Paint / Backend speculation.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HAP_DIR="$ROOT/host/fw2-hap"
UNSIGNED="$HAP_DIR/entry/build/default/outputs/default/entry-default-unsigned.hap"
EV="$ROOT/docs/logs/minimal-boot"
BUNDLE="com.codelite.fw2.host"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
mkdir -p "$EV"

"$HAP_DIR/scripts/stage-wx-libs.sh"
"$HAP_DIR/scripts/stage-runtime-assets.sh"
# Re-apply F-UI binary patches + refresh HAP libs (stage-wx-libs overwrites pristine wx).
bash "$HAP_DIR/scripts/rebuild-wx-fui3-3ef-boot-menu.sh"

if [[ ! -f "$UNSIGNED" ]]; then
  echo "[!] HAP not built: $UNSIGNED"
  exit 1
fi

echo "[*] hdc list targets:"
"$HDC" list targets 2>&1 || true
T=$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[!] No device. Start MateBook emulator first."
  echo "    Do NOT guess B-n. Boot Dashboard stays all ⏳."
  exit 1
fi
echo "[*] Device: $T"

"$HDC" -t "$T" install "$UNSIGNED"
"$HDC" -t "$T" shell "aa force-stop $BUNDLE" || true
# B4-002: Runtime Assets are inside HAP rawfile; Host extracts to filesDir on launch.
echo "[*] Runtime Assets: HAP rawfile → Host deploy (filesDir/share/codelite)"

"$HDC" -t "$T" shell "hilog -r" || true
"$HDC" -t "$T" shell "aa start -a EntryAbility -b $BUNDLE"
# rawfile extract + BOOT-001 phase-2 Alive ≥5s
sleep 20

STAMP=$(date +%Y%m%d-%H%M%S)
RAW="$EV/hilog-raw-$STAMP.txt"
OUT="$EV/hilog-$STAMP.txt"
"$HDC" -t "$T" shell hilog -x 2>/dev/null | tee "$RAW" >/dev/null
grep -E '\[B-[1-8](\.[0-9])?\]|\[MV-[1-4](\.[0-9])?\]|\[P-[1-4](\.[0-9])?\]|\[R-[1-7](\.[0-9])?\]|\[WS-[1-6]\]|\[R-paint\]|\[R-present\]|\[Present\]|FW2Host|A0f002|A0f004|CodeLiteBoot|CodeLiteWS|wxOHOS|EmbeddedStart|CodeLiteApp|MainFrame|Minimal Boot|MENU_DEPLOY|ToolBar|Workspace' "$RAW" \
  | tee "$OUT" | tail -240 || true

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Original CodeLite Boot"
echo ""
FIRST_FAIL=""

boot_mark() {
  local id="$1" label="$2"
  if grep -q "\[${id}\] FAIL" "$OUT" 2>/dev/null; then
    printf "%-28s ❌\n" "$id $label"
    [[ -z "$FIRST_FAIL" ]] && FIRST_FAIL="$id"
  elif grep -q "\[${id}\]" "$OUT" 2>/dev/null; then
    # For existence probes, require exists=1 when present in line
    if grep "\[${id}\]" "$OUT" 2>/dev/null | grep -q 'exists=0'; then
      printf "%-28s 🟡\n" "$id $label"
      [[ -z "$FIRST_FAIL" ]] && FIRST_FAIL="$id"
    else
      printf "%-28s ✅\n" "$id $label"
    fi
  else
    printf "%-28s ⏳\n" "$id $label"
    [[ -z "$FIRST_FAIL" ]] && FIRST_FAIL="$id"
  fi
}

boot_mark "B-1" "EmbeddedStart"
boot_mark "B-2" "wxEntry"
boot_mark "B-3" "CodeLiteApp::OnInit"
boot_mark "B-4.1" "InstallDir"
boot_mark "B-4.2" "DataDir"
boot_mark "B-4.3" "Load RC / menu.xrc"
boot_mark "B-4.4" "Config"
boot_mark "B-4.5" "Images"
boot_mark "B-4.6" "Resources OK"
boot_mark "B-5" "Plugins Initialized"
boot_mark "B-6" "clMainFrame Created"
boot_mark "B-7" "clMainFrame Shown"
boot_mark "B-8.1" "wxApp::OnRun"
boot_mark "B-8.2" "EventLoop::Run"
boot_mark "B-8.3" "first Dispatch/CallAfter"
boot_mark "BOOT-2.1" "MainLoop resident"
boot_mark "BOOT-2.2" "first Idle"
boot_mark "BOOT-2.3" "wxTimer"
boot_mark "BOOT-2.4" "Alive ≥5s"

echo ""
echo "MainFrame Visible"
boot_mark "MV-1" "official clMainFrame TopWindow"
boot_mark "MV-2" "OHNativeWindow Attach 1:1"
boot_mark "MV-3" "Show() on TopWindow"
boot_mark "MV-4.1" "NativeWindow surface"
boot_mark "MV-4.2" "eglCreateWindowSurface"
boot_mark "MV-4.3" "eglMakeCurrent"
boot_mark "MV-4.4" "first pixel (SwapBuffers)"
boot_mark "P-1" "BeginPaint / PaintEvent"
boot_mark "P-2" "BackingStore / clientSize"
boot_mark "P-3.1" "wxBitmap RGBA buffer"
boot_mark "P-3.2" "MemoryDC Select"
boot_mark "P-3.3" "Bitmap pixels changed"
boot_mark "P-3.4" "CPU Present from Bitmap"

echo ""
echo "Render Dashboard"
boot_mark_ok() {
  local id="$1" label="$2"
  if grep -q "\[${id}\] FAIL" "$OUT" 2>/dev/null; then
    printf "%-28s ❌\n" "$id $label"
  elif grep -q "\[${id}\] OK" "$OUT" 2>/dev/null; then
    printf "%-28s ✅\n" "$id $label"
  else
    printf "%-28s ⏳\n" "$id $label"
  fi
}
boot_mark_ok "R-1" "RendererNative → Bitmap"
boot_mark_ok "R-2" "Basic Controls"
boot_mark_ok "R-3" "MainFrame BG"
boot_mark_ok "R-4" "MenuBar"
boot_mark_ok "R-5.1" "Toolbar"
boot_mark_ok "WS-1" "AuiManager"
boot_mark_ok "WS-2" "Workspace Pane"
boot_mark_ok "WS-3" "Pane Layout"
boot_mark_ok "WS-4" "Tree Create"
boot_mark_ok "WS-5" "Tree/Page Paint"
boot_mark_ok "WS-6" "Workspace Visible"
boot_mark "R-5.3" "Editor Notebook"
boot_mark "R-5.4" "Bottom Dock"
echo "P-4 eye (Menu/Tool/Dock/Editor) ⏳"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "[*] Evidence: $OUT"
MV_FAIL=""
for id in "MV-1" "MV-2" "MV-3" "MV-4.1" "MV-4.2" "MV-4.3" "MV-4.4" "P-1"; do
  if grep -q "\[${id}\] FAIL" "$OUT" 2>/dev/null; then
    MV_FAIL="$id"
    break
  fi
  if ! grep -q "\[${id}\] OK" "$OUT" 2>/dev/null && ! grep -q "\[${id}\]" "$OUT" 2>/dev/null; then
    MV_FAIL="$id"
    break
  fi
done
for id in "R-1" "R-2" "R-3" "R-4" "R-5.1" "WS-1" "WS-2" "WS-3" "WS-4" "WS-5" "WS-6"; do
  if [[ -z "$MV_FAIL" ]] && ! grep -q "\[${id}\] OK" "$OUT" 2>/dev/null; then
    MV_FAIL="$id"
  fi
done
if [[ -n "$MV_FAIL" ]]; then
  echo "[*] Current Blocker: $MV_FAIL  ← fix ONLY this (official render path)"
elif grep -q '\[WS-6\] OK' "$OUT" 2>/dev/null; then
  echo "[*] R-5.2 ✅ — next R-5.3 Editor Notebook eye (screenshot)"
elif grep -q '\[R-5.1\] OK' "$OUT" 2>/dev/null; then
  echo "[*] R-5.1 ✅ — next R-5.2 Workspace"
else
  echo "[*] see docs/render-dashboard.md"
fi
