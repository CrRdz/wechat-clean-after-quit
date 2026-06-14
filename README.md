# WeChat Clean After Quit

Automatically deletes local WeChat desktop chat records after WeChat quits.

## Files

- `wechat-clean-after-quit.sh`: cleanup script and lightweight shell watcher.
- `WeChatCleanAfterQuit.c`: native background watcher and cleaner.
- `WeChatCleanAfterQuit.app`: helper app bundle for launchd and Full Disk Access.
- `com.crzhu.wechat-clean-after-quit.plist`: launchd config.

## What It Cleans

- `wxid_*/msg`
- `wxid_*/db_storage/message`
- `wxid_*/db_storage/session`
- `wxid_*/temp`
- `wxid_*/cache`
- `WMPF`

It keeps login, config, contacts, favorites, stickers, and similar baseline data.

## Why There Is a Helper App

macOS privacy controls can block a LaunchAgent shell script from deleting files inside
WeChat's container, even when the same script works from Terminal. The native helper app
is the preferred background entrypoint because it can be granted Full Disk Access and it
does the cleanup directly instead of delegating deletion to `/bin/rm`.

## Commands

Build the native helper:

```bash
cc -Wall -Wextra -O2 -o /Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.app/Contents/MacOS/WeChatCleanAfterQuit /Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.c
codesign --force --deep --sign - /Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.app
```

Grant Full Disk Access:

1. Open System Settings > Privacy & Security > Full Disk Access.
2. Add `/Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.app`.
3. Turn it on. If the helper is rebuilt or re-signed, toggle it off and on again.

Preview:

```bash
/Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.app/Contents/MacOS/WeChatCleanAfterQuit --dry-run
```

Clean once:

```bash
/Users/crzhu/Library/wechat-clean-after-quit/WeChatCleanAfterQuit.app/Contents/MacOS/WeChatCleanAfterQuit --yes
```

Install or reload the background watcher:

```bash
launchctl bootout gui/$(id -u) /Users/crzhu/Library/LaunchAgents/com.crzhu.wechat-clean-after-quit.plist 2>/dev/null
launchctl bootstrap gui/$(id -u) /Users/crzhu/Library/LaunchAgents/com.crzhu.wechat-clean-after-quit.plist
```

Check status:

```bash
launchctl print gui/$(id -u)/com.crzhu.wechat-clean-after-quit
```

View logs:

```bash
tail -50 /Users/crzhu/Library/Logs/wechat-clean-after-quit.log
```

Uninstall:

```bash
launchctl bootout gui/$(id -u) /Users/crzhu/Library/LaunchAgents/com.crzhu.wechat-clean-after-quit.plist
```
