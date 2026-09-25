#!/usr/bin/env bash
# Boot smoke test: build the kernel, boot it in QEMU, and check that
# user/init.S printed its fork/wait lines on the serial console.
#
# The kernel never powers off, so QEMU is killed once every expected line
# has appeared, or when TIMEOUT seconds pass.
#
# Env overrides:
#   CROSS    toolchain prefix (default: riscv32-unknown-elf- if present,
#            otherwise riscv64-unknown-elf-, which also targets rv32)
#   TIMEOUT  seconds to wait for the expected output (default 30)
#   EXPECT   space-separated lines that must appear, in order
#            (default "Parent Child Reaped")
set -euo pipefail

cd "$(dirname "$0")/.."

if [ -z "${CROSS:-}" ]; then
    if command -v riscv32-unknown-elf-gcc >/dev/null; then
        CROSS=riscv32-unknown-elf-
    else
        CROSS=riscv64-unknown-elf-
    fi
fi
TIMEOUT=${TIMEOUT:-30}
read -r -a EXPECT <<< "${EXPECT:-Parent Child Reaped}"

make clean >/dev/null
make CC="${CROSS}gcc" OBJCOPY="${CROSS}objcopy"

log=$(mktemp)
trap 'kill "$qemu_pid" 2>/dev/null || true; rm -f "$log"' EXIT

qemu-system-riscv32 -machine virt -bios none -kernel kernel.elf -nographic \
    </dev/null >"$log" 2>&1 &
qemu_pid=$!

# Every expected line must appear, and in the given order.
check() {
    local from=0 n
    for want in "${EXPECT[@]}"; do
        n=$(tail -n +"$((from + 1))" "$log" | tr -d '\r' | grep -n -x -m1 -F "$want" | cut -d: -f1) || return 1
        [ -n "$n" ] || return 1
        from=$((from + n))
    done
}

deadline=$((SECONDS + TIMEOUT))
until check; do
    if ! kill -0 "$qemu_pid" 2>/dev/null || [ "$SECONDS" -ge "$deadline" ]; then
        echo "--- serial output ---"
        cat "$log"
        echo "---------------------"
        echo "FAIL: expected '${EXPECT[*]}' in order within ${TIMEOUT}s"
        exit 1
    fi
    sleep 0.2
done

echo "--- serial output ---"
cat "$log"
echo "---------------------"
echo "PASS: saw '${EXPECT[*]}' in order"
