#!/bin/bash

get_time_ms() {
    # Try GNU date (Linux)
    t=$(date +%s%3N 2>/dev/null)
    if [[ "$t" =~ ^[0-9]+$ ]]; then
        echo "$t"
        return
    fi

    # Try Perl (macOS / most systems)
    if command -v perl >/dev/null 2>&1; then
        perl -MTime::HiRes=time -e 'printf("%.0f\n", time()*1000)'
        return
    fi

    # Try Python (Linux fallback)
    if command -v python3 >/dev/null 2>&1; then
        python3 -c 'import time; print(int(time.time()*1000))'
        return
    fi

    # Last fallback (seconds only)
    echo $(( $(date +%s) * 1000 ))
}


set -e

# START_TIME=$(date +%s%3N)
START_TIME=$(get_time_ms)


echo
echo "+---------------------------------+"
printf "| %-31s |\n" "[BUILD] Starting build..."
printf "| %-31s |\n" "[TIME ] $(date '+%Y-%m-%d %H:%M:%S')"
echo "+---------------------------------+"

# Compile
if gcc \
    src/constants/globals.c \
    src/services/init_pache.c \
    src/services/tcp_server.c \
    src/main.c \
    -Iinclude -o pache.out -lpthread; then
    printf "| %-31s |\n" "[SUCCESS] Compilation completed"
    echo "+---------------------------------+"
else
    printf "| %-31s |\n" "[ERROR] Compilation failed."
    echo "+---------------------------------+"
    exit 1
fi

# END_TIME=$(date +%s%3N)
END_TIME=$(get_time_ms)
DURATION_MS=$((END_TIME - START_TIME))

SECONDS=$((DURATION_MS / 1000))
MILLIS=$((DURATION_MS % 1000))

DURATION_FMT=$(printf "%d.%03ds" "$SECONDS" "$MILLIS")

printf "| %-31s |\n" "[BUILD] Finished"
printf "| %-31s |\n" "[TIME ] $(date '+%Y-%m-%d %H:%M:%S')"
printf "| %-31s |\n" "[TOOK ] ${DURATION_FMT}"
echo "+---------------------------------+"
echo
