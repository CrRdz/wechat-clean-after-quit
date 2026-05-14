# WeChat Clean After Quit

Automatically deletes local WeChat desktop chat records after WeChat quits.

## Files

- `wechat-clean-after-quit.sh`: cleanup script and lightweight watcher.
- `../LaunchAgents/com.crzhu.wechat-clean-after-quit.plist`: launchd config.

## What It Cleans

- `wxid_*/msg`
- `wxid_*/db_storage/message`
- `wxid_*/db_storage/session`
- `wxid_*/temp`
- `wxid_*/cache`
- `WMPF`

It keeps login, config, contacts, favorites, stickers, and similar baseline data.

## Commands

Preview:

```bash
/Users/crzhu/Library/wechat-clean-after-quit/wechat-clean-after-quit.sh --dry-run
```

Clean once:

```bash
/Users/crzhu/Library/wechat-clean-after-quit/wechat-clean-after-quit.sh --yes
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
