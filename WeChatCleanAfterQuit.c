#define _DARWIN_C_SOURCE

#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *REL_XFILES = "/Library/Containers/com.tencent.xinWeChat/Data/Documents/xwechat_files";
static const char *REL_LOG = "/Library/Logs/wechat-clean-after-quit.log";
static char xfiles[PATH_MAX];
static char log_path[PATH_MAX];

static void timestamp(char *buf, size_t len) {
  time_t now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);
  strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm_now);
}

static void log_line(const char *fmt, ...) {
  char parent[PATH_MAX];
  snprintf(parent, sizeof(parent), "%s", log_path);
  char *slash = strrchr(parent, '/');
  if (slash) {
    *slash = '\0';
    mkdir(parent, 0755);
  }

  FILE *fp = fopen(log_path, "a");
  if (!fp) return;

  char ts[32];
  timestamp(ts, sizeof(ts));
  fprintf(fp, "[%s] ", ts);

  va_list args;
  va_start(args, fmt);
  vfprintf(fp, fmt, args);
  va_end(args);

  fputc('\n', fp);
  fclose(fp);
}

static bool is_dir(const char *path) {
  struct stat st;
  return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void human_size(const char *path, char *out, size_t out_len) {
  char cmd[PATH_MAX + 64];
  snprintf(cmd, sizeof(cmd), "/usr/bin/du -sh '%s' 2>/dev/null", path);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    snprintf(out, out_len, "?");
    return;
  }

  if (!fgets(out, (int)out_len, fp)) {
    snprintf(out, out_len, "?");
  } else {
    char *space = strpbrk(out, " \t\n");
    if (space) *space = '\0';
  }
  pclose(fp);
}

static bool wechat_running(void) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
  size_t len = 0;
  if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) return false;

  struct kinfo_proc *procs = malloc(len);
  if (!procs) return false;
  if (sysctl(mib, 4, procs, &len, NULL, 0) != 0) {
    free(procs);
    return false;
  }

  int count = (int)(len / sizeof(struct kinfo_proc));
  bool running = false;
  for (int i = 0; i < count; i++) {
    if (strcmp(procs[i].kp_proc.p_comm, "WeChat") == 0) {
      running = true;
      break;
    }
  }

  free(procs);
  return running;
}

static void prepare_tree(const char *path) {
  struct stat st;
  if (lstat(path, &st) != 0) return;

  chmod(path, st.st_mode | S_IRUSR | S_IWUSR | S_IXUSR);
  removexattr(path, "com.apple.quarantine", XATTR_NOFOLLOW);

  if (!S_ISDIR(st.st_mode)) return;

  DIR *dir = opendir(path);
  if (!dir) return;

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

    char child[PATH_MAX];
    snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
    prepare_tree(child);
  }

  closedir(dir);
}

static int remove_tree(const char *path) {
  struct stat st;
  if (lstat(path, &st) != 0) return errno == ENOENT ? 0 : -1;

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

      char child[PATH_MAX];
      snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
      if (remove_tree(child) != 0) {
        int saved = errno;
        closedir(dir);
        errno = saved;
        return -1;
      }
    }

    closedir(dir);
    return rmdir(path);
  }

  return unlink(path);
}

static void add_target(char targets[][PATH_MAX], int *count, const char *path) {
  if (*count >= 256 || !is_dir(path)) return;
  snprintf(targets[*count], PATH_MAX, "%s", path);
  (*count)++;
}

static int clean_once(bool dry_run) {
  if (!is_dir(xfiles)) {
    log_line("WeChat data directory not found: %s", xfiles);
    return 0;
  }

  if (wechat_running()) {
    log_line("WeChat is still running. Quit WeChat first.");
    return 1;
  }

  char targets[256][PATH_MAX];
  int target_count = 0;

  DIR *dir = opendir(xfiles);
  if (dir) {
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
      if (strncmp(ent->d_name, "wxid_", 5) != 0) continue;

      char account[PATH_MAX];
      snprintf(account, sizeof(account), "%s/%s", xfiles, ent->d_name);
      if (!is_dir(account)) continue;

      const char *subdirs[] = {"msg", "db_storage/message", "db_storage/session", "temp", "cache"};
      for (size_t i = 0; i < sizeof(subdirs) / sizeof(subdirs[0]); i++) {
        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s/%s", account, subdirs[i]);
        add_target(targets, &target_count, target);
      }
    }
    closedir(dir);
  }

  char wmpf[PATH_MAX];
  snprintf(wmpf, sizeof(wmpf), "%s/WMPF", xfiles);
  add_target(targets, &target_count, wmpf);

  if (target_count == 0) {
    log_line("No local chat-record folders found.");
    return 0;
  }

  log_line("Local WeChat chat-record candidates:");
  for (int i = 0; i < target_count; i++) {
    char size[32];
    human_size(targets[i], size, sizeof(size));
    log_line("  %s  %s", size, targets[i]);
  }

  if (dry_run) {
    log_line("Dry run only. Nothing moved.");
    return 0;
  }

  int failed = 0;
  for (int i = 0; i < target_count; i++) {
    log_line("Deleting: %s", targets[i]);
    prepare_tree(targets[i]);
    if (remove_tree(targets[i]) != 0) {
      failed = 1;
      log_line("Failed to delete: %s", targets[i]);
      log_line("  %s", strerror(errno));
    }
  }

  if (failed) {
    log_line("Done with errors. Some scoped local WeChat chat records could not be deleted.");
    return 1;
  }

  log_line("Done. Deleted scoped local WeChat chat records.");
  return 0;
}

static void watch_loop(void) {
  log_line("native watcher started");
  bool was_running = wechat_running();
  if (!was_running) {
    clean_once(false);
  }

  while (true) {
    if (wechat_running()) {
      was_running = true;
    } else if (was_running) {
      log_line("WeChat quit detected");
      clean_once(false);
      was_running = false;
    }

    sleep(15);
  }
}

int main(int argc, char **argv) {
  const char *home = getenv("HOME");
  if (!home) home = "/Users/crzhu";

  snprintf(xfiles, sizeof(xfiles), "%s%s", home, REL_XFILES);
  snprintf(log_path, sizeof(log_path), "%s%s", home, REL_LOG);

  bool watch = false;
  bool dry_run = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--watch") == 0) watch = true;
    else if (strcmp(argv[i], "--dry-run") == 0) dry_run = true;
    else if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {}
    else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      return 1;
    }
  }

  if (watch) {
    watch_loop();
    return 0;
  }

  return clean_once(dry_run);
}
