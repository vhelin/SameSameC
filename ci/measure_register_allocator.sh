#!/bin/bash

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
output=${RA_BENCHMARK_DIR:-$(mktemp -d)}
mkdir -p "$output"
output=$(cd "$output" && pwd)
export LC_ALL=C
export TIMEFORMAT='RA_COMPILE_SECONDS=%R'

if [ "$#" -eq 0 ]; then
  set -- calculations-1 calculations-2 allocator_ra_array_read_reload allocator_ra_array_write_pointer_bases allocator_ra_shift_loop_results allocator_ra_core_liveness_extraction allocator_ra_core_equal_next_use_policy
fi

printf 'fixture\tmode\trom_bytes\tframe_total_bytes\tframe_max_bytes\tinserted_spills\tinserted_reloads\tregister_copies\tcompile_seconds\tworkspace_plan_bytes\n' > "$output/summary.tsv"
for fixture in "$@"; do
  directory="$root/tests/z80/sms/$fixture"
  test -f "$directory/makefile"
  objects=$(make -s -C "$directory" --eval='ra_objects: ; @echo $(OFILES)' ra_objects)
  test -n "$objects"
  asm_files=()
  for object in $objects; do
    asm_files+=("$directory/$object")
  done
  for mode in default no_ra all_spill ra; do
    flags=''
    case "$mode" in
      no_ra) flags='-no-ra' ;;
      all_spill) flags='-ra-all-spill' ;;
      ra) flags='-ra' ;;
    esac
    log="$output/$fixture.$mode.log"
    make -s -C "$directory" clean linked.sms SHELL=/bin/bash CC='time samesamecc' CFLAGS="$flags -mz80 -rs 3 -o" > "$log" 2>&1 || {
      cat "$log" >&2
      exit 1
    }
    for compiler_log in "$directory"/compiler.log "$directory"/*.compiler.log; do
      if [ -f "$compiler_log" ]; then
        cat "$compiler_log" >> "$log"
      fi
    done
    sha256sum "${asm_files[@]}" > "$output/$fixture.$mode.sha256"
    mkdir -p "$output/$fixture.$mode"
    cp "${asm_files[@]}" "$directory/linked.sms" "$output/$fixture.$mode/"
    awk -v fixture="$fixture" -v mode="$mode" '
      FILENAME ~ /\.asm$/ {
        if (/allocate space for the stack frame/) frame = 1
        else if (frame && /LD[[:space:]]+HL,-[0-9]+/) {
          bytes = $0; sub(/.*HL,-/, "", bytes); bytes += 0
          frame_total += bytes
          if (bytes > frame_max) frame_max = bytes
          frame = 0
        }
        if (/; TAC: spill |preserve retained .*spill slot/) spills++
        if (/; reload .*from spill slot/) reloads++
        if ($0 ~ /^[[:space:]]*LD[[:space:]]+(A|B|C|D|E|H|L|SP|IX|IY),(A|B|C|D|E|H|L|SP|IX|IY)[[:space:]]*$/) copies++
        next
      }
      /^ROM: .* free of total / { rom = $8 - $2; rom_found = 1 }
      /^RA_COMPILE_SECONDS=/ { seconds = $0; sub(/.*=/, "", seconds); compile_seconds += seconds; timings++ }
      /register_allocator_core: .*_storage .*status=complete/ {
        for (field = 1; field <= NF; field++) {
          split($field, pair, "=")
          if (pair[1] == "total_bytes" || pair[1] == "bytes" ||
              pair[1] == "instruction_bytes" || pair[1] == "block_bytes" ||
              pair[1] == "edge_bytes" || pair[1] == "execution_bytes") workspace += pair[2]
        }
      }
      END {
        if (!rom_found || rom < 0 || !timings) exit 1
        printf "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%.0f\n", fixture, mode, rom,
          frame_total, frame_max, spills, reloads, copies, compile_seconds, workspace
      }
    ' "${asm_files[@]}" "$log" >> "$output/summary.tsv"
  done
done
cat "$output/summary.tsv"
printf 'Logs and assembly hashes: %s\n' "$output" >&2