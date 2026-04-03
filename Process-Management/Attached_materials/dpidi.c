#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef __WALL
#define __WALL 0x40000000
#endif

#define MAX_NODES 4096
#define MAX_CHILDREN 128
#define STRSZ 4096

static const long TRACE_OPTS =
    PTRACE_O_TRACEFORK |
    PTRACE_O_TRACEVFORK |
    PTRACE_O_TRACECLONE |
    PTRACE_O_TRACEEXEC |
    PTRACE_O_TRACEEXIT |
    PTRACE_O_EXITKILL;

typedef struct Node {
    pid_t pid;
    pid_t ppid;
    int used;
    int creation_index;
    int exited;
    int saw_exec;
    int exit_code;
    int exit_by_signal;
    int stop_signal;
    int child_count;
    pid_t children[MAX_CHILDREN];
    char comm[256];
    char cmdline[STRSZ];
    char role[STRSZ];
    char fd0[STRSZ];
    char fd1[STRSZ];
    char fd2[STRSZ];
} Node;

static Node nodes[MAX_NODES];
static int node_count = 0;
static int creation_counter = 0;

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int find_node_index(pid_t pid) {
    for (int i = 0; i < node_count; ++i) {
        if (nodes[i].used && nodes[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

static Node *get_or_create_node(pid_t pid) {
    int idx = find_node_index(pid);
    if (idx >= 0) {
        return &nodes[idx];
    }
    if (node_count >= MAX_NODES) {
        die("Too many traced processes (limit %d)", MAX_NODES);
    }
    Node *n = &nodes[node_count++];
    memset(n, 0, sizeof(*n));
    n->used = 1;
    n->pid = pid;
    n->ppid = -1;
    n->creation_index = creation_counter++;
    snprintf(n->fd0, sizeof(n->fd0), "unknown");
    snprintf(n->fd1, sizeof(n->fd1), "unknown");
    snprintf(n->fd2, sizeof(n->fd2), "unknown");
    snprintf(n->comm, sizeof(n->comm), "pid-%d", pid);
    snprintf(n->cmdline, sizeof(n->cmdline), "pid-%d", pid);
    snprintf(n->role, sizeof(n->role), "pid-%d", pid);
    return n;
}

static void add_child_relation(pid_t ppid, pid_t child) {
    Node *p = get_or_create_node(ppid);
    for (int i = 0; i < p->child_count; ++i) {
        if (p->children[i] == child) {
            return;
        }
    }
    if (p->child_count < MAX_CHILDREN) {
        p->children[p->child_count++] = child;
    }
}

static int read_text_file(const char *path, char *buf, size_t bufsz) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    ssize_t n = read(fd, buf, bufsz - 1);
    close(fd);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    return (int)n;
}

static void trim_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static void read_proc_comm(pid_t pid, char *out, size_t outsz) {
    char path[64];
    char buf[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (read_text_file(path, buf, sizeof(buf)) >= 0) {
        trim_newline(buf);
        snprintf(out, outsz, "%s", buf);
    }
}

static void read_proc_cmdline(pid_t pid, char *out, size_t outsz) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return;
    }
    ssize_t n = read(fd, out, outsz - 1);
    close(fd);
    if (n <= 0) {
        return;
    }
    out[n] = '\0';
    for (ssize_t i = 0; i < n - 1; ++i) {
        if (out[i] == '\0') {
            out[i] = ' ';
        }
    }
    trim_newline(out);
}

static int read_fd_target(pid_t pid, int fdnum, char *out, size_t outsz) {
    char path[64];
    char buf[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fdnum);
    ssize_t n = readlink(path, buf, sizeof(buf) - 1);
    if (n < 0) {
        return -1;
    }
    buf[n] = '\0';
    snprintf(out, outsz, "%s", buf);
    return 0;
}

static void update_role(Node *n, const char *root_script_name, pid_t root_pid) {
    if (n->pid == root_pid) {
        snprintf(n->role, sizeof(n->role), "%s (shell)", root_script_name);
        return;
    }

    if (strcmp(n->comm, "bash") == 0 || strcmp(n->comm, "sh") == 0 || strcmp(n->comm, "dash") == 0) {
        if (n->saw_exec) {
            snprintf(n->role, sizeof(n->role), "%s", n->cmdline[0] ? n->cmdline : n->comm);
        } else {
            snprintf(n->role, sizeof(n->role), "%s (shell child)", n->comm);
        }
        return;
    }

    if (n->cmdline[0]) {
        char tmp[STRSZ];
        snprintf(tmp, sizeof(tmp), "%s", n->cmdline);
        char *sp = strchr(tmp, ' ');
        if (sp) {
            *sp = '\0';
        }
        snprintf(n->role, sizeof(n->role), "%s", base_name(tmp));
        return;
    }

    snprintf(n->role, sizeof(n->role), "%s", n->comm);
}

static void snapshot_process(pid_t pid, const char *root_script_name, pid_t root_pid) {
    Node *n = get_or_create_node(pid);
    read_proc_comm(pid, n->comm, sizeof(n->comm));
    read_proc_cmdline(pid, n->cmdline, sizeof(n->cmdline));
    if (!n->cmdline[0]) {
        snprintf(n->cmdline, sizeof(n->cmdline), "%s", n->comm);
    }
    char tmp[STRSZ];
    if (read_fd_target(pid, 0, tmp, sizeof(tmp)) == 0) {
        snprintf(n->fd0, sizeof(n->fd0), "%s", tmp);
    }
    if (read_fd_target(pid, 1, tmp, sizeof(tmp)) == 0) {
        snprintf(n->fd1, sizeof(n->fd1), "%s", tmp);
    }
    if (read_fd_target(pid, 2, tmp, sizeof(tmp)) == 0) {
        snprintf(n->fd2, sizeof(n->fd2), "%s", tmp);
    }
    update_role(n, root_script_name, root_pid);
}

static void set_trace_options_if_possible(pid_t pid) {
    if (ptrace(PTRACE_SETOPTIONS, pid, 0, (void *)TRACE_OPTS) == -1) {
        // Ignore transient failures (e.g. just exited), but keep tracing.
    }
}

static int compare_children_by_creation(const void *a, const void *b) {
    pid_t pa = *(const pid_t *)a;
    pid_t pb = *(const pid_t *)b;
    int ia = find_node_index(pa);
    int ib = find_node_index(pb);
    int ca = (ia >= 0) ? nodes[ia].creation_index : 0;
    int cb = (ib >= 0) ? nodes[ib].creation_index : 0;
    return (ca > cb) - (ca < cb);
}

static void print_tree_children(pid_t pid, const char *prefix) {
    int idx = find_node_index(pid);
    if (idx < 0) {
        return;
    }
    Node *n = &nodes[idx];
    if (n->child_count == 0) {
        return;
    }

    pid_t sorted[MAX_CHILDREN];
    for (int i = 0; i < n->child_count; ++i) {
        sorted[i] = n->children[i];
    }
    qsort(sorted, n->child_count, sizeof(sorted[0]), compare_children_by_creation);

    for (int i = 0; i < n->child_count; ++i) {
        int cidx = find_node_index(sorted[i]);
        if (cidx < 0) continue;
        Node *c = &nodes[cidx];
        int is_last = (i == n->child_count - 1);
        printf("%s%s── %d (%s)\n", prefix, is_last ? "└" : "├", c->pid, c->role);

        char next_prefix[STRSZ];
        snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, is_last ? "    " : "│   ");
        print_tree_children(c->pid, next_prefix);
    }
}

static void print_tree(pid_t root_pid) {
    int idx = find_node_index(root_pid);
    if (idx < 0) return;
    Node *n = &nodes[idx];
    printf("%d (%s)\n", n->pid, n->role);
    print_tree_children(root_pid, "");
}

static void print_fd_report(void) {
    Node *ordered[MAX_NODES];
    int count = 0;
    for (int i = 0; i < node_count; ++i) {
        if (nodes[i].used) {
            ordered[count++] = &nodes[i];
        }
    }
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (ordered[j]->creation_index < ordered[i]->creation_index) {
                Node *tmp = ordered[i];
                ordered[i] = ordered[j];
                ordered[j] = tmp;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        Node *n = ordered[i];
        printf("%d | %s | fd 0: %s | fd 1: %s\n", n->pid, n->role, n->fd0, n->fd1);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <script.sh>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *script = argv[1];
    const char *script_name = base_name(script);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
            perror("ptrace(TRACEME)");
            _exit(127);
        }
        raise(SIGSTOP);
        execl("/bin/bash", "bash", script, (char *)NULL);
        perror("execl(/bin/bash)");
        _exit(127);
    }

    int status;
    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if (!WIFSTOPPED(status)) {
        die("Unexpected initial child state");
    }

    Node *root = get_or_create_node(child);
    root->ppid = 0;
    snapshot_process(child, script_name, child);
    set_trace_options_if_possible(child);

    int live = 1;
    if (ptrace(PTRACE_CONT, child, 0, 0) == -1) {
        perror("ptrace(CONT initial)");
        return EXIT_FAILURE;
    }

    while (live > 0) {
        pid_t pid = waitpid(-1, &status, __WALL);
        if (pid < 0) {
            if (errno == ECHILD) {
                break;
            }
            perror("waitpid(__WALL)");
            return EXIT_FAILURE;
        }

        Node *n = get_or_create_node(pid);
        snapshot_process(pid, script_name, child);

        if (WIFEXITED(status)) {
            n->exited = 1;
            n->exit_code = WEXITSTATUS(status);
            live--;
            continue;
        }
        if (WIFSIGNALED(status)) {
            n->exited = 1;
            n->exit_by_signal = WTERMSIG(status);
            live--;
            continue;
        }
        if (!WIFSTOPPED(status)) {
            continue;
        }

        int sig = WSTOPSIG(status);
        int event = status >> 16;
        set_trace_options_if_possible(pid);

        if (sig == SIGTRAP) {
            if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK || event == PTRACE_EVENT_CLONE) {
                unsigned long newpid = 0;
                if (ptrace(PTRACE_GETEVENTMSG, pid, 0, &newpid) == 0 && newpid > 0) {
                    Node *child_node = get_or_create_node((pid_t)newpid);
                    child_node->ppid = pid;
                    add_child_relation(pid, (pid_t)newpid);
                    snapshot_process((pid_t)newpid, script_name, child);
                    live++;
                }
            } else if (event == PTRACE_EVENT_EXEC) {
                n->saw_exec = 1;
                snapshot_process(pid, script_name, child);
            } else if (event == PTRACE_EVENT_EXIT) {
                snapshot_process(pid, script_name, child);
            }

            if (ptrace(PTRACE_CONT, pid, 0, 0) == -1 && errno != ESRCH) {
                perror("ptrace(CONT trap)");
                return EXIT_FAILURE;
            }
            continue;
        }

        n->stop_signal = sig;
        if (ptrace(PTRACE_CONT, pid, 0, (void *)(long)sig) == -1 && errno != ESRCH) {
            perror("ptrace(CONT signal)");
            return EXIT_FAILURE;
        }
    }

    printf("Дерево зависимых PID:\n");
    print_tree(child);
    printf("\nДанные по вводу-выводу:\n");
    print_fd_report();

    return EXIT_SUCCESS;
}
