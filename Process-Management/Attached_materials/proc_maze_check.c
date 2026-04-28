#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SECRET_FILE_NAME ".secrets-maze"
#define PID_FILE "/tmp/linux_practic_process_maze.pids"
#define MARKER "LinuxPracticUsage"

struct ProcEntry {
    long pid;
    int node;
    char role[64];
};

struct ZombieEntry {
    long pid;
    long parent;
};

struct MazeMap {
    struct ProcEntry *procs;
    size_t procs_len;
    size_t procs_cap;
    struct ZombieEntry *zombies;
    size_t zombies_len;
    size_t zombies_cap;
};

struct Stats {
    int arg_markers;
    int env_markers;
    int file_markers;
    int deleted_file_markers;
    int zombie_processes;
    int stopped_processes;
    int hardstop_processes;
    int name_markers;
    int other_special_processes;
};

static int secret_path(char *out, size_t out_size) {
    const char *home = getenv("HOME");
    if (!home || !*home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0]) home = pw->pw_dir;
    }
    if (!home || !*home) home = "/tmp";
    int n = snprintf(out, out_size, "%s/%s", home, SECRET_FILE_NAME);
    return n > 0 && (size_t)n < out_size;
}

static const char *get_value(const char *line, const char *key, char *buf, size_t buf_size) {
    size_t key_len = strlen(key);
    const char *p = line;
    while ((p = strstr(p, key)) != NULL) {
        if ((p == line || isspace((unsigned char)p[-1])) && p[key_len] == '=') {
            p += key_len + 1;
            size_t i = 0;
            while (p[i] && !isspace((unsigned char)p[i]) && i + 1 < buf_size) {
                buf[i] = p[i];
                i++;
            }
            buf[i] = '\0';
            return buf;
        }
        p += key_len;
    }
    return NULL;
}

static void add_proc(struct MazeMap *map, long pid, int node, const char *role) {
    if (map->procs_len == map->procs_cap) {
        size_t new_cap = map->procs_cap ? map->procs_cap * 2 : 64;
        struct ProcEntry *p = realloc(map->procs, new_cap * sizeof(*p));
        if (!p) return;
        map->procs = p;
        map->procs_cap = new_cap;
    }
    map->procs[map->procs_len].pid = pid;
    map->procs[map->procs_len].node = node;
    snprintf(map->procs[map->procs_len].role, sizeof(map->procs[map->procs_len].role), "%s", role);
    map->procs_len++;
}

static void add_zombie(struct MazeMap *map, long pid, long parent) {
    if (map->zombies_len == map->zombies_cap) {
        size_t new_cap = map->zombies_cap ? map->zombies_cap * 2 : 8;
        struct ZombieEntry *z = realloc(map->zombies, new_cap * sizeof(*z));
        if (!z) return;
        map->zombies = z;
        map->zombies_cap = new_cap;
    }
    map->zombies[map->zombies_len].pid = pid;
    map->zombies[map->zombies_len].parent = parent;
    map->zombies_len++;
}

static int load_secret(struct MazeMap *map, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PROC ", 5) == 0) {
            char pid_s[64] = "", node_s[64] = "", role_s[64] = "";
            if (get_value(line, "pid", pid_s, sizeof(pid_s)) &&
                get_value(line, "node", node_s, sizeof(node_s)) &&
                get_value(line, "role", role_s, sizeof(role_s))) {
                add_proc(map, strtol(pid_s, NULL, 10), (int)strtol(node_s, NULL, 10), role_s);
            }
        } else if (strncmp(line, "ZOMBIE ", 7) == 0) {
            char pid_s[64] = "", parent_s[64] = "";
            if (get_value(line, "pid", pid_s, sizeof(pid_s)) &&
                get_value(line, "parent", parent_s, sizeof(parent_s))) {
                add_zombie(map, strtol(pid_s, NULL, 10), strtol(parent_s, NULL, 10));
            }
        }
    }
    fclose(f);
    return 0;
}

static int proc_exists(long pid) {
    if (pid <= 0) return 0;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%ld", pid);
    return access(path, F_OK) == 0;
}

static char proc_state(long pid) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%ld/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return '\0';
    char line[256];
    char state = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "State:", 6) == 0) {
            char *p = line + 6;
            while (*p && isspace((unsigned char)*p)) p++;
            state = *p;
            break;
        }
    }
    fclose(f);
    return state;
}

static ssize_t read_file_bytes(const char *path, char **out) {
    *out = NULL;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap + 1);
    if (!buf) {
        close(fd);
        return -1;
    }
    for (;;) {
        if (len == cap) {
            cap *= 2;
            char *nbuf = realloc(buf, cap + 1);
            if (!nbuf) {
                free(buf);
                close(fd);
                return -1;
            }
            buf = nbuf;
        }
        ssize_t r = read(fd, buf + len, cap - len);
        if (r < 0) {
            free(buf);
            close(fd);
            return -1;
        }
        if (r == 0) break;
        len += (size_t)r;
        if (cap > 1024 * 1024) break;
    }
    close(fd);
    buf[len] = '\0';
    *out = buf;
    return (ssize_t)len;
}

static int file_contains_bytes(const char *path, const char *needle) {
    char *buf = NULL;
    ssize_t len = read_file_bytes(path, &buf);
    if (len <= 0 || !buf) {
        free(buf);
        return 0;
    }
    int found = memmem(buf, (size_t)len, needle, strlen(needle)) != NULL;
    free(buf);
    return found;
}

static int proc_cmd_contains(long pid, const char *needle) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    return file_contains_bytes(path, needle);
}

static int proc_env_contains(long pid, const char *needle) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%ld/environ", pid);
    return file_contains_bytes(path, needle);
}

static int proc_comm_contains(long pid, const char *needle) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
    return file_contains_bytes(path, needle);
}

static int proc_fd_links_contain(long pid, const char *needle, int require_deleted) {
    char dirpath[128];
    snprintf(dirpath, sizeof(dirpath), "/proc/%ld/fd", pid);
    DIR *d = opendir(dirpath);
    if (!d) return 0;
    struct dirent *de;
    int found = 0;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char linkpath[512];
        char target[4096];
        snprintf(linkpath, sizeof(linkpath), "%s/%s", dirpath, de->d_name);
        ssize_t n = readlink(linkpath, target, sizeof(target) - 1);
        if (n <= 0) continue;
        target[n] = '\0';
        if (strstr(target, needle) && (!require_deleted || strstr(target, "(deleted)"))) {
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

static int is_special_role(const char *role) {
    return role && strcmp(role, "NONE") != 0;
}

static int stats_total(const struct Stats *s) {
    return s->arg_markers + s->env_markers + s->file_markers + s->deleted_file_markers +
           s->zombie_processes + s->stopped_processes + s->hardstop_processes +
           s->name_markers + s->other_special_processes;
}

static void collect_stats(const struct MazeMap *map, struct Stats *s) {
    memset(s, 0, sizeof(*s));

    for (size_t i = 0; i < map->zombies_len; ++i) {
        if (proc_exists(map->zombies[i].pid) && proc_state(map->zombies[i].pid) == 'Z') {
            s->zombie_processes++;
        }
    }

    for (size_t i = 0; i < map->procs_len; ++i) {
        const struct ProcEntry *p = &map->procs[i];
        if (!proc_exists(p->pid)) continue;
        char state = proc_state(p->pid);
        if (state == 'Z') continue;

        if (strcmp(p->role, "ARG_MARKER") == 0) {
            if (proc_cmd_contains(p->pid, MARKER)) s->arg_markers++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "ENV_MARKER") == 0) {
            if (proc_env_contains(p->pid, MARKER)) s->env_markers++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "OUTPUT_MARKER") == 0) {
            if (proc_fd_links_contain(p->pid, MARKER, 0)) s->file_markers++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "DELETED_FILE") == 0) {
            if (proc_fd_links_contain(p->pid, MARKER, 1)) s->deleted_file_markers++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "IGNORE_TERM") == 0) {
            if (proc_env_contains(p->pid, MARKER)) s->hardstop_processes++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "ZOMBIE_PARENT") == 0) {
            int has_zombie = 0;
            for (size_t z = 0; z < map->zombies_len; ++z) {
                if (map->zombies[z].parent == p->pid && proc_exists(map->zombies[z].pid) && proc_state(map->zombies[z].pid) == 'Z') {
                    has_zombie = 1;
                    break;
                }
            }
            if (!has_zombie) s->other_special_processes++;
        } else if (strcmp(p->role, "STOPPED") == 0) {
            if (proc_state(p->pid) == 'T') s->stopped_processes++;
            else s->other_special_processes++;
        } else if (strcmp(p->role, "NAME_MARKER") == 0) {
            if (proc_comm_contains(p->pid, "LinuxPractic") || proc_cmd_contains(p->pid, MARKER)) s->name_markers++;
            else s->other_special_processes++;
        } else if (is_special_role(p->role)) {
            s->other_special_processes++;
        }
    }
}

static void print_pending(const struct Stats *s) {
    puts("Результат: лабиринт ещё не побеждён.");
    puts("Статистика оставшихся категорий без PID и подсказок:");
    if (s->arg_markers)          printf("- процессы с секретными метками в аргументах: %d\n", s->arg_markers);
    if (s->env_markers)          printf("- процессы с секретными метками в окружении: %d\n", s->env_markers);
    if (s->file_markers)         printf("- файловые дескрипторы с секретными метками: %d\n", s->file_markers);
    if (s->deleted_file_markers) printf("- удалённые файлы, удерживаемые процессами: %d\n", s->deleted_file_markers);
    if (s->stopped_processes)    printf("- остановленные процессы: %d\n", s->stopped_processes);
    if (s->hardstop_processes)   printf("- процессы, не завершающиеся по SIGTERM: %d\n", s->hardstop_processes);
    if (s->name_markers)         printf("- процессы с секретными метками в имени: %d\n", s->name_markers);
    if (s->other_special_processes) printf("- прочие специальные процессы: %d\n", s->other_special_processes);
}

static void cleanup_remaining_maze_processes(const struct MazeMap *map) {
    size_t live_count = 0;
    for (size_t i = 0; i < map->procs_len; ++i) {
        long pid = map->procs[i].pid;
        if (pid > 0 && pid != (long)getpid() && proc_exists(pid)) live_count++;
    }

    if (live_count == 0) {
        puts("Фиктивных процессов для очистки не осталось.");
        unlink(PID_FILE);
        return;
    }

    printf("Очищаю оставшиеся фиктивные процессы: %zu\n", live_count);

    for (size_t i = 0; i < map->procs_len; ++i) {
        long pid = map->procs[i].pid;
        if (pid > 0 && pid != (long)getpid() && proc_exists(pid)) kill((pid_t)pid, SIGCONT);
    }
    usleep(200000);

    for (size_t i = 0; i < map->procs_len; ++i) {
        long pid = map->procs[i].pid;
        if (pid > 0 && pid != (long)getpid() && proc_exists(pid)) kill((pid_t)pid, SIGTERM);
    }
    sleep(1);

    size_t survivors = 0;
    for (size_t i = 0; i < map->procs_len; ++i) {
        long pid = map->procs[i].pid;
        if (pid > 0 && pid != (long)getpid() && proc_exists(pid)) {
            kill((pid_t)pid, SIGKILL);
            survivors++;
        }
    }
    if (survivors) {
        printf("SIGKILL отправлен оставшимся фиктивным процессам: %zu\n", survivors);
        sleep(1);
    }
    unlink(PID_FILE);
}

static void free_map(struct MazeMap *map) {
    free(map->procs);
    free(map->zombies);
    memset(map, 0, sizeof(*map));
}

int main(void) {
    char path[4096];
    if (!secret_path(path, sizeof(path))) {
        return 21;
    }

    struct MazeMap map = {0};
    if (load_secret(&map, path) != 0) {
        fprintf(stderr, "Сначала запустите ./proc_maze_stand\n");
        return 22;
    }

    if (map.procs_len == 0) {
        free_map(&map);
        return 23;
    }

    struct Stats stats;
    collect_stats(&map, &stats);

    if (stats_total(&stats) == 0) {
        puts("Результат: успех. Все специальные процессы побеждены.");
        cleanup_remaining_maze_processes(&map);
        puts("Проверка завершена.");
        free_map(&map);
        return 0;
    }

    print_pending(&stats);
    free_map(&map);
    return 1;
}
