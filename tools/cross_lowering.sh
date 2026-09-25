#!/bin/sh
# Cross-generate ONLY the two real queue source files. No SDK/link/run claim.
set -eu
cd "$(dirname "$0")/.."
CLANG=${CLANG:-clang}
OUT=${1:-evidence/assembly}
mkdir -p "$OUT"
RESOURCE=$("$CLANG" -print-resource-dir)
"$CLANG" --version > "$OUT/compiler.txt"
for profile in armv8 armv81-lse darwin-armv8; do
    case "$profile" in
      armv8) target=aarch64-none-elf; arch=armv8-a ;;
      armv81-lse) target=aarch64-none-elf; arch=armv8.1-a+lse ;;
      darwin-armv8) target=arm64-apple-macos14.4; arch=armv8-a ;;
    esac
    for unit in elite_spsc elite_mpmc_ncq; do
        "$CLANG" --target="$target" -march="$arch" -mno-outline-atomics \
          -std=c11 -O3 -Wall -Wextra -Werror -pedantic -ffreestanding \
          -nostdinc -isystem "$RESOURCE/include" -Itools/cross_headers \
          -Iinclude -Isrc -S "src/$unit.c" -o "$OUT/$profile-$unit.s"
    done
done
# An unavailable requested CPU is recorded as unavailable, never substituted.
if "$CLANG" --target=arm64-apple-macos14.4 -mcpu=apple-m4 -std=c11 -O3 \
    -Wall -Wextra -Werror -pedantic -ffreestanding -nostdinc \
    -isystem "$RESOURCE/include" -Itools/cross_headers -Iinclude -Isrc \
    -S src/elite_spsc.c -o "$OUT/apple-m4-elite_spsc.s" \
    > "$OUT/apple-m4-status.txt" 2>&1; then
    echo "ASSEMBLY_ONLY_ACCEPTED; NOT SDK-LINKED OR EXECUTED" >> "$OUT/apple-m4-status.txt"
else
    echo "UNSUPPORTED_BY_INSTALLED_COMPILER; NO M4 CODEGEN CLAIM" >> "$OUT/apple-m4-status.txt"
fi

# Explicit RCpc-disabled profile realizes the requested LDAR/STLR form even
# when the CPU tuning defaults otherwise select LDAPR. Compiler-specific;
# failing flag support or failed mnemonic audit rejects that build tuple.
for unit in elite_spsc elite_mpmc_ncq; do
    "$CLANG" --target=arm64-apple-macos14.4 -mcpu=apple-m4 \
      -Xclang -target-feature -Xclang -rcpc -std=c11 -O3 \
      -Wall -Wextra -Werror -pedantic -ffreestanding -nostdinc \
      -isystem "$RESOURCE/include" -Itools/cross_headers -Iinclude -Isrc \
      -S "src/$unit.c" -o "$OUT/apple-m4-norcpc-$unit.s"
done
if grep -Eq '[[:space:]]ldapr[[:space:]]' "$OUT/apple-m4-norcpc-elite_spsc.s"; then
    echo 'FAIL: strict LDAR profile emitted LDAPR' >&2; exit 1
fi
grep -Eq '[[:space:]]ldar[[:space:]]' "$OUT/apple-m4-norcpc-elite_spsc.s"
grep -Eq '[[:space:]]stlr[[:space:]]' "$OUT/apple-m4-norcpc-elite_spsc.s"
echo 'PASS strict M4 source-to-assembly profile; native SDK/link/run NOT_TESTED'
