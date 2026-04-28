#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_MAX_NODES 63
#define PID_FILE "/tmp/linux_practic_process_maze.pids"
#define SECRET_FILE_NAME ".secrets-maze"
#define MAX_NAME 15
#define MARKER "LinuxPracticUsage"

static const char *OUTPUT_MARKER_PATH = "/tmp/LinuxPracticUsage-shadow-output.log";
static const char *DELETED_MARKER_PATH = "/tmp/LinuxPracticUsage-deleted-handle.tmp";

enum SpecialRole {
    ROLE_NONE = 0,
    ROLE_ARG_MARKER,
    ROLE_ENV_MARKER,
    ROLE_OUTPUT_MARKER,
    ROLE_DELETED_FILE,
    ROLE_IGNORE_TERM,
    ROLE_ZOMBIE_PARENT,
    ROLE_STOPPED,
    ROLE_NAME_MARKER
};

static const char *syllables[] = {
    "ax", "bel", "cor", "dun", "ev", "far", "gri", "hal",
    "ion", "jor", "ka", "lim", "mor", "nel", "or", "pra",
    "qua", "rin", "sol", "tur", "ul", "vor", "wen", "xim",
    "yor", "zen"
};

static int parse_int(const char *s, int fallback) {
    if (!s || !*s) return fallback;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0') return fallback;
    if (v < 0 || v > 1000000) return fallback;
    return (int)v;
}

static enum SpecialRole role_for_node(int id) {
    switch (id) {
        case 9:  return ROLE_ARG_MARKER;
        case 14: return ROLE_ENV_MARKER;
        case 22: return ROLE_OUTPUT_MARKER;
        case 27: return ROLE_DELETED_FILE;
        case 33: return ROLE_IGNORE_TERM;
        case 38: return ROLE_ZOMBIE_PARENT;
        case 44: return ROLE_STOPPED;
        case 51: return ROLE_NAME_MARKER;
        default: return ROLE_NONE;
    }
}

static const char *role_name(enum SpecialRole role) {
    switch (role) {
        case ROLE_ARG_MARKER:    return "ARG_MARKER";
        case ROLE_ENV_MARKER:    return "ENV_MARKER";
        case ROLE_OUTPUT_MARKER: return "OUTPUT_MARKER";
        case ROLE_DELETED_FILE:  return "DELETED_FILE";
        case ROLE_IGNORE_TERM:   return "IGNORE_TERM";
        case ROLE_ZOMBIE_PARENT: return "ZOMBIE_PARENT";
        case ROLE_STOPPED:       return "STOPPED";
        case ROLE_NAME_MARKER:   return "NAME_MARKER";
        case ROLE_NONE:
        default:                 return "NONE";
    }
}

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

static void append_secret_line(const char *fmt, ...) {
    char path[4096];
    if (!secret_path(path, sizeof(path))) return;

    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (len <= 0) return;
    if ((size_t)len >= sizeof(line)) {
        len = (int)sizeof(line) - 2;
        line[len++] = '\n';
        line[len] = '\0';
    }

    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (fd < 0) return;
    (void)write(fd, line, (size_t)len);
    close(fd);
}

static void init_secret_file(int seed, int max_nodes) {
    char path[4096];
    if (!secret_path(path, sizeof(path))) return;
    unlink(path);

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) {
        fprintf(stderr, "Cannot create secret file %s: %s\n", path, strerror(errno));
        return;
    }

    time_t now = time(NULL);
    dprintf(fd, "# Secret process maze map. Do not show this file to students during the investigation.\n");
    dprintf(fd, "VERSION 2\n");
    dprintf(fd, "ROOT pid=%ld seed=%d nodes=%d started=%ld pid_file=%s marker=%s\n",
            (long)getpid(), seed, max_nodes, (long)now, PID_FILE, MARKER);
    dprintf(fd, "PATH output=%s deleted=%s\n", OUTPUT_MARKER_PATH, DELETED_MARKER_PATH);
    close(fd);
    chmod(path, 0600);
}

static void make_name(int id, int seed, char *out, size_t out_size) {
    size_t n = sizeof(syllables) / sizeof(syllables[0]);
    unsigned int x = (unsigned int)(seed * 1103515245u + id * 2654435761u + 12345u);
    const char *a = syllables[(x >> 3) % n];
    const char *b = syllables[(x >> 9) % n];
    const char *c = syllables[(x >> 15) % n];
    const char *suffixes[] = {"d", "q", "h", "r", "x", "m"};
    const char *s = suffixes[(x >> 21) % 6];
    snprintf(out, out_size, "%s%s%s%s", a, b, c, s);
    out[MAX_NAME] = '\0';
}

static void record_pid(int id, int seed, const char *comm) {
    enum SpecialRole role = role_for_node(id);

    int fd = open(PID_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) {
        char line[160];
        int len = snprintf(line, sizeof(line), "%ld node=%d seed=%d role=%s\n",
                           (long)getpid(), id, seed, role_name(role));
        if (len > 0) (void)write(fd, line, (size_t)len);
        close(fd);
    }

    append_secret_line("PROC pid=%ld node=%d seed=%d role=%s comm=%s\n",
                       (long)getpid(), id, seed, role_name(role), comm ? comm : "unknown");
}

static void setup_devnull(void) {
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) return;
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
}

static void setup_output_marker(void) {
    char path[4096];
    snprintf(path, sizeof(path), "%s", OUTPUT_MARKER_PATH);
    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) {
        snprintf(path, sizeof(path), "/tmp/LinuxPracticUsage-shadow-output-%ld.log", (long)getuid());
        fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    }
    if (fd < 0) {
        setup_devnull();
        return;
    }
    dprintf(fd, "[%ld] quiet writer attached to %s\n", (long)getpid(), path);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
}

static void setup_deleted_file_marker(void) {
    char path[4096];
    snprintf(path, sizeof(path), "%s", DELETED_MARKER_PATH);
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
        snprintf(path, sizeof(path), "/tmp/LinuxPracticUsage-deleted-handle-%ld.tmp", (long)getuid());
        fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    }
    if (fd < 0) return;
    dprintf(fd, "This file was unlinked but is still open by PID %ld\n", (long)getpid());
    unlink(path);
    /* Keep fd open forever. */
}

static void make_zombie_child(int seed, int parent_node) {
    pid_t z = fork();
    if (z < 0) return;
    if (z == 0) {
        char zname[32];
        make_name(900 + (getpid() % 97), seed, zname, sizeof(zname));
        prctl(PR_SET_NAME, zname, 0, 0, 0);
        _exit(0);
    }

    append_secret_line("ZOMBIE pid=%ld parent=%ld parent_node=%d seed=%d role=ZOMBIE_CHILD\n",
                       (long)z, (long)getpid(), parent_node, seed);
    /* Intentionally do not wait(). The child remains zombie while this parent lives. */
}

static char *self_path(void) {
    char *buf = calloc(1, 4096);
    if (!buf) return NULL;
    ssize_t n = readlink("/proc/self/exe", buf, 4095);
    if (n <= 0) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    return buf;
}

static void spawn_node(int child_id, int seed, int max_nodes) {
    char *exe = self_path();
    if (!exe) return;

    char name[32];
    make_name(child_id, seed, name, sizeof(name));

    char id_buf[32], seed_buf[32], max_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%d", child_id);
    snprintf(seed_buf, sizeof(seed_buf), "%d", seed);
    snprintf(max_buf, sizeof(max_buf), "%d", max_nodes);

    enum SpecialRole role = role_for_node(child_id);

    char arg0[64];
    if (role == ROLE_NAME_MARKER) {
        snprintf(arg0, sizeof(arg0), "LinuxPracticUsage-agent");
    } else {
        snprintf(arg0, sizeof(arg0), "%s", name);
    }

    char *argv[10];
    int ai = 0;
    argv[ai++] = arg0;
    argv[ai++] = "--relay";
    argv[ai++] = id_buf;
    argv[ai++] = seed_buf;
    argv[ai++] = max_buf;
    if (role == ROLE_ARG_MARKER) {
        argv[ai++] = "--profile=LinuxPracticUsage";
    }
    argv[ai] = NULL;

    char home_value[4096];
    char home_env[4102];
    if (!secret_path(home_value, sizeof(home_value))) {
        snprintf(home_value, sizeof(home_value), "/tmp/%s", SECRET_FILE_NAME);
    }
    char *slash = strrchr(home_value, '/');
    if (slash) *slash = '\0';
    snprintf(home_env, sizeof(home_env), "HOME=%s", home_value);

    char *envp[12];
    int ei = 0;
    envp[ei++] = "PATH=/usr/sbin:/usr/bin:/sbin:/bin";
    envp[ei++] = "LANG=C";
    envp[ei++] = home_env;
    if (role == ROLE_ENV_MARKER) {
        envp[ei++] = "LPU_TOKEN=LinuxPracticUsage";
        envp[ei++] = "LPU_HINT=environment";
    }
    if (role == ROLE_IGNORE_TERM) {
        envp[ei++] = "LPU_HARDSTOP=LinuxPracticUsage";
    }
    envp[ei] = NULL;

    pid_t p = fork();
    if (p < 0) {
        free(exe);
        return;
    }
    if (p == 0) {
        execve(exe, argv, envp);
        _exit(127);
    }
    free(exe);
}

static void quiet_forever(void) {
    for (;;) pause();
}

static void run_node(int id, int seed, int max_nodes) {
    enum SpecialRole role = role_for_node(id);

    char comm[32];
    if (role == ROLE_NAME_MARKER) {
        snprintf(comm, sizeof(comm), "LinuxPractic");
    } else {
        make_name(id, seed, comm, sizeof(comm));
    }
    prctl(PR_SET_NAME, comm, 0, 0, 0);

    record_pid(id, seed, comm);

    if (role == ROLE_OUTPUT_MARKER) {
        setup_output_marker();
    } else {
        setup_devnull();
    }
    if (role == ROLE_DELETED_FILE) setup_deleted_file_marker();
    if (role == ROLE_IGNORE_TERM) signal(SIGTERM, SIG_IGN);
    if (role == ROLE_ZOMBIE_PARENT) make_zombie_child(seed, id);

    int left = id * 2 + 1;
    int right = id * 2 + 2;
    if (left < max_nodes) spawn_node(left, seed, max_nodes);
    if (right < max_nodes) spawn_node(right, seed, max_nodes);

    if (role == ROLE_STOPPED) raise(SIGSTOP);

    quiet_forever();
}

static void usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s [--seed N] [--nodes N]\n", prog);
    printf("\n");
    printf("Starts a quiet process tree for the Linux process investigation lab.\n");
    printf("PID file for emergency cleanup: %s\n", PID_FILE);
    printf("Secret check file: ~/.secrets-maze\n");
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--relay") == 0) {
        if (argc < 5) return 2;
        int id = parse_int(argv[2], 0);
        int seed = parse_int(argv[3], 2026);
        int max_nodes = parse_int(argv[4], DEFAULT_MAX_NODES);
        run_node(id, seed, max_nodes);
        return 0;
    }

    int seed = (int)(time(NULL) ^ getpid());
    int max_nodes = DEFAULT_MAX_NODES;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = parse_int(argv[++i], seed);
            continue;
        }
        if (strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            max_nodes = parse_int(argv[++i], max_nodes);
            if (max_nodes < 15) max_nodes = 15;
            if (max_nodes > 127) max_nodes = 127;
            continue;
        }
    }

    unlink(PID_FILE);
    init_secret_file(seed, max_nodes);

    printf("Process maze started. Root PID: %ld, seed: %d, nodes: %d\n", (long)getpid(), seed, max_nodes);
    printf("Student checker: ./proc_maze_check\n");
    printf("Emergency cleanup: ./proc_maze_cleanup.sh\n");
    fflush(stdout);

    run_node(0, seed, max_nodes);
    return 0;
}
