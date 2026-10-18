#!/bin/bash

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
output=${RA_BENCHMARK_DIR:-$(mktemp -d)}
mkdir -p "$output"
output=$(cd "$output" && pwd)
export LC_ALL=C
export TIMEFORMAT='RA_COMPILE_SECONDS=%R'
repeats=${RA_BENCHMARK_REPEATS:-1}
case "$repeats" in ''|*[!0-9]*) echo 'RA_BENCHMARK_REPEATS must be 1..31' >&2; exit 1 ;; esac
test "$repeats" -ge 1 && test "$repeats" -le 31
timing_cc='time samesamecc'
if [ -n "${RA_TIME:-}" ]; then
  test -x "$RA_TIME"
  timing_cc="$RA_TIME -f 'RA_COMPILE_SECONDS=%e RA_PEAK_RSS_KIB=%M' samesamecc"
fi

if [ "$#" -eq 0 ]; then
  set -- calculations-1 calculations-2 allocator_ra_array_read_reload allocator_ra_array_write_pointer_bases allocator_ra_shift_loop_results allocator_ra_core_liveness_extraction allocator_ra_core_equal_next_use_policy
fi

printf 'fixture\tmode\trom_bytes\tframe_total_bytes\tframe_max_bytes\tinserted_spills\tinserted_reloads\tregister_copies\tcompile_seconds\tworkspace_plan_bytes\tcompile_min_seconds\tcompile_max_seconds\tcompile_samples\tpeak_rss_kib\tlog_bytes\trejected_decisions\n' > "$output/summary.tsv"
for fixture in "$@"; do
  directory="$root/tests/z80/sms/$fixture"
  test -f "$directory/makefile"
  objects=$(make -s -C "$directory" --eval='ra_objects: ; @echo $(OFILES)' ra_objects)
  test -n "$objects"
  asm_files=()
  for object in $objects; do
    asm_files+=("$directory/$object")
  done
  for mode in default no_ra all_spill ra ra_quiet; do
    flags=''
    case "$mode" in
      no_ra) flags='-no-ra' ;;
      all_spill) flags='-ra-all-spill' ;;
      ra) flags='-ra' ;;
      ra_quiet) flags='-ra -q' ;;
    esac
    samples="$output/$fixture.$mode.samples.tsv"
    : > "$samples"
    mkdir -p "$output/$fixture.$mode"
    for ((sample = 1; sample <= repeats; sample++)); do
      log="$output/$fixture.$mode.$sample.log"
      make -s -C "$directory" clean linked.sms SHELL=/bin/bash CC="$timing_cc" CFLAGS="$flags -mz80 -rs 3 -o" > "$log" 2>&1 || {
        cat "$log" >&2
        exit 1
      }
      for compiler_log in "$directory"/compiler.log "$directory"/*.compiler.log; do
        if [ -f "$compiler_log" ]; then
          cat "$compiler_log" >> "$log"
        fi
      done
      if [ "$mode" = ra ] && ! grep -q '^register_allocator_core: liveness_solve .*status=converged' "$log"; then
        echo 'Allocator workspace/rejection measurements require a trace-enabled compiler (CMake -DSAMESAMEC_TRACE=ON or make TRACE=1; rebuild cleanly).' >&2
        exit 1
      fi
      awk -v sample="$sample" -v require_peak="${RA_TIME:-}" -v log_bytes="$(wc -c < "$log")" '
        /^RA_COMPILE_SECONDS=/ { split($1, timing, "="); seconds += timing[2]; timings++ }
        /RA_PEAK_RSS_KIB=/ { split($2, memory, "="); if (memory[2] > peak) peak = memory[2] }
        END { if (!timings || (require_peak != "" && peak <= 0)) exit 1;
          printf "%d\t%.3f\t%.0f\t%d\n", sample, seconds, peak, log_bytes }
      ' "$log" >> "$samples"
      if [ "$sample" -eq 1 ]; then
        cp "${asm_files[@]}" "$directory/linked.sms" "$output/$fixture.$mode/"
      else
        cmp "$directory/linked.sms" "$output/$fixture.$mode/linked.sms"
      fi
    done
    metrics=$(awk '
      { values[NR] = $2; if ($3 > peak) peak = $3 }
      END {
        for (sample_index = 2; sample_index <= NR; sample_index++) {
          value = values[sample_index]; previous = sample_index - 1;
          while (previous > 0 && values[previous] > value) { values[previous + 1] = values[previous]; previous-- }
          values[previous + 1] = value
        }
        median = (values[int((NR + 1) / 2)] + values[int((NR + 2) / 2)]) / 2;
        printf "%.3f %.3f %.3f %s", values[1], median, values[NR], (peak > 0 ? peak : "unavailable")
      }
    ' "$samples")
    read -r minimum median maximum peak <<< "$metrics"
    sha256sum "${asm_files[@]}" > "$output/$fixture.$mode.sha256"
    awk -v fixture="$fixture" -v mode="$mode" -v median="$median" -v minimum="$minimum" -v maximum="$maximum" -v repeats="$repeats" -v peak="$peak" -v log_bytes="$(wc -c < "$log")" '
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
      /^RA_COMPILE_SECONDS=/ { timings++ }
      /^register_allocator: rejection_summary / {
        for (field = 1; field <= NF; field++) {
          split($field, pair, "=")
          if (pair[1] == "unique_decisions") rejected += pair[2]
        }
      }
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
        printf "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%.0f\t%.3f\t%.3f\t%d\t%s\t%d\t%d\n", fixture, mode, rom,
          frame_total, frame_max, spills, reloads, copies, median, workspace, minimum, maximum, repeats, peak, log_bytes, rejected
      }
    ' "${asm_files[@]}" "$log" >> "$output/summary.tsv"
  done
done
cat "$output/summary.tsv"
printf 'Logs and assembly hashes: %s\n' "$output" >&2