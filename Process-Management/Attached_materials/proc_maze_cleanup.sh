#!/bin/sh
# Emergency cleanup script for the process investigation stand.
# It uses the PID file created by proc_maze_stand and filters stale PIDs by cmdline.
set -eu
PID_FILE=/tmp/linux_practic_process_maze.pids
SECRET_FILE=${HOME:-/tmp}/.secrets-maze

if [ ! -f "$PID_FILE" ]; then
    echo "PID file not found: $PID_FILE"
    echo "Try manual search: ps -eo pid,ppid,pgid,stat,comm,args --forest"
    [ -f "$SECRET_FILE" ] && echo "Secret file exists: $SECRET_FILE"
    exit 1
fi

PIDS=""
for pid in $(awk '{print $1}' "$PID_FILE" | sort -n | uniq); do
    case "$pid" in
        ''|*[!0-9]*) continue ;;
    esac
    [ "$pid" = "$$" ] && continue
    [ ! -d "/proc/$pid" ] && continue
    cmd=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)
    case "$cmd" in
        *--relay*|*proc_maze_stand*|*LinuxPracticUsage*) PIDS="$PIDS $pid" ;;
    esac
    # Some zombie children have empty cmdline; keep PIDs that are still recorded in the stand PID file out of this filter only indirectly.
done

if [ -z "$PIDS" ]; then
    echo "No live stand processes found. Removing stale PID file."
    rm -f "$PID_FILE"
    exit 0
fi

echo "Live stand PIDs:$PIDS"
echo "Sending SIGCONT to stopped processes..."
kill -CONT $PIDS 2>/dev/null || true
sleep 1

echo "Sending SIGTERM..."
kill -TERM $PIDS 2>/dev/null || true
sleep 2

SURVIVORS=""
for pid in $PIDS; do
    [ -d "/proc/$pid" ] && SURVIVORS="$SURVIVORS $pid"
done

if [ -n "$SURVIVORS" ]; then
    echo "Sending SIGKILL to survivors:$SURVIVORS"
    kill -KILL $SURVIVORS 2>/dev/null || true
    sleep 1
fi

rm -f "$PID_FILE"
echo "Cleanup attempted. Remaining suspicious processes, if any:"
ps -eo pid,ppid,stat,comm,args | grep -E 'LinuxPracticUsage|--relay|proc_maze_stand' | grep -v grep || true
