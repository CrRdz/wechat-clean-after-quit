#!/usr/bin/env zsh
set -euo pipefail
setopt NULL_GLOB

XFILES="$HOME/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files"
LOG="$HOME/Library/Logs/wechat-clean-after-quit.log"

DRY_RUN=0
YES=0
WATCH=0
POLL_SECONDS=15

usage() {
  cat <<'EOF'
Usage:
  ./wechat-clean-after-quit.sh --dry-run
  ./wechat-clean-after-quit.sh --yes
  ./wechat-clean-after-quit.sh --watch

This removes local WeChat desktop chat records after WeChat has quit.
It permanently deletes the scoped local records below. Use --dry-run first.

Cleaned:
  wxid_*/msg
  wxid_*/db_storage/message
  wxid_*/db_storage/session
  wxid_*/temp
  wxid_*/cache
  WMPF

Kept:
  login/config/contact/favorite/emoticon data, so WeChat should remain usable.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run) DRY_RUN=1 ;;
    --yes|-y) YES=1 ;;
    --watch) WATCH=1 ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
  shift
done

timestamp() {
  date '+%Y-%m-%d %H:%M:%S'
}

log() {
  mkdir -p "$(dirname "$LOG")"
  local line="[$(timestamp)] $*"
  echo "$line" >> "$LOG"
  if [[ -t 1 ]]; then
    echo "$line"
  fi
  return 0
}

human_size() {
  local size
  size=$(/usr/bin/du -sh "$1" 2>/dev/null | /usr/bin/awk '{print $1}' || true)
  echo "${size:-?}"
}

wechat_running() {
  pgrep -x WeChat >/dev/null 2>&1
}

clean_once() {
  if [[ ! -d "$XFILES" ]]; then
    log "WeChat data directory not found: $XFILES"
    return 0
  fi

  if wechat_running; then
    log "WeChat is still running. Quit WeChat first."
    return 1
  fi

  local targets=()
  local account subdir target
  while IFS= read -r account; do
    [[ -d "$account" ]] || continue
    for subdir in msg db_storage/message db_storage/session temp cache; do
      [[ -d "$account/$subdir" ]] && targets+=("$account/$subdir")
    done
  done < <(/usr/bin/find "$XFILES" -maxdepth 1 -type d -name 'wxid_*' -print 2>/dev/null)

  [[ -d "$XFILES/WMPF" ]] && targets+=("$XFILES/WMPF")

  if [[ "${#targets[@]}" -eq 0 ]]; then
    log "No local chat-record folders found."
    return 0
  fi

  log "Local WeChat chat-record candidates:"
  for target in "${targets[@]}"; do
    log "  $(human_size "$target")  $target"
  done

  if [[ "$DRY_RUN" -eq 1 ]]; then
    log "Dry run only. Nothing moved."
    return 0
  fi

  if [[ "$YES" -ne 1 ]]; then
    echo
    printf "Permanently delete these WeChat local chat records? Type CLEAN to proceed: "
    read -r answer
    [[ "$answer" == "CLEAN" ]] || { log "Cancelled."; return 0; }
  fi

  local failed=0
  for target in "${targets[@]}"; do
    log "Deleting: $target"
    /bin/chmod -R u+rwX "$target" >/dev/null 2>&1 || true
    /usr/bin/xattr -dr com.apple.quarantine "$target" >/dev/null 2>&1 || true
    if ! error=$(/bin/rm -rf "$target" 2>&1); then
      failed=1
      log "Failed to delete: $target"
      log "  $error"
    fi
  done

  if [[ "$failed" -eq 0 ]]; then
    log "Done. Deleted scoped local WeChat chat records."
  else
    log "Done with errors. Some scoped local WeChat chat records could not be deleted."
  fi
}

watch_loop() {
  mkdir -p "$(dirname "$LOG")"
  log "watcher started"

  local was_running=0
  if wechat_running; then
    was_running=1
  else
    YES=1 clean_once || true
  fi

  while true; do
    if wechat_running; then
      was_running=1
    elif [[ "$was_running" -eq 1 ]]; then
      log "WeChat quit detected"
      YES=1 clean_once || true
      was_running=0
    fi
    sleep "$POLL_SECONDS"
  done
}

if [[ "$WATCH" -eq 1 ]]; then
  watch_loop
else
  clean_once
fi
