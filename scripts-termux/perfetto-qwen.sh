#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# -----------------------------------------------------------------------------
# CPU-only Qwen run + Perfetto trace capture
#
# Usage:
#   ./scripts-termux/ignite-qwen.sh <cpu_prefill_idx> <ram_prefill_idx> <cpu_decode_idx> <ram_decode_idx>
#
# Optional env:
#   DEV_OVERRIDE=S25   # device label (for backlight + core control path selection)
#   N_THREADS=4        # stream_qwen -t (CPU threads)
#
# Output (repo_root/output):
#   - Trace:        output/perfetto_*.pftrace
#   - Perfetto log: output/perfetto_*.log
#   - Snapshots:    output/freq_snapshot_*.txt
#   - Config copy:  output/perfetto_cfg_*.pbtxt
# -----------------------------------------------------------------------------

if [[ $# -lt 4 ]]; then
  echo "Usage: $0 <cpu_prefill_idx> <ram_prefill_idx> <cpu_decode_idx> <ram_decode_idx>" >&2
  exit 2
fi

CPU_P="$1"
RAM_P="$2"
CPU_D="$3"
RAM_D="$4"

N_THREADS=4

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

OUTPUT_DIR="$REPO_ROOT/output"
mkdir -p "$OUTPUT_DIR"

# 기존 스크립트가 사실상 S25 고정이었으니, 기본도 S25로 둠.
DEV="${DEV_OVERRIDE:-S25}"
echo "Device: $DEV"

PERFETTO_BIN="/system/bin/perfetto"
if [[ ! -x "$PERFETTO_BIN" ]]; then
  echo "ERROR: perfetto not found at $PERFETTO_BIN" >&2
  exit 1
fi

# Detect tracefs: prefer /sys/kernel/tracing, fallback /sys/kernel/debug/tracing.
TRACEFS="/sys/kernel/tracing"
if [[ ! -d "$TRACEFS" ]]; then
  TRACEFS="/sys/kernel/debug/tracing"
fi

# Best-effort: mount tracefs if missing.
if [[ ! -d "$TRACEFS" ]]; then
  su -c "mkdir -p /sys/kernel/tracing; mount -t tracefs tracefs /sys/kernel/tracing" >/dev/null 2>&1 || true
  TRACEFS="/sys/kernel/tracing"
fi

# Turn-off screen (backlight path differs by device).
if [[ "$DEV" == "Pixel9" || "$DEV" == "S24" || "$DEV" == "S25" ]]; then
  su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness" || true
else
  su -c "echo 0 > /sys/class/backlight/panel/brightness" || true
fi

# CPU governor -> performance (policy6 may not exist on all devices).
su -c "echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor" || true
su -c "echo performance > /sys/devices/system/cpu/cpufreq/policy6/scaling_governor" || true
sleep 3

# Silver core control (Except S25): offline cpu1~cpu3.
SILVER_OFFLINED=0
if [[ "$DEV" != "S25" ]]; then
  for c in 1 2 3; do
    su -c "echo 0 > /sys/devices/system/cpu/cpu${c}/online" >/dev/null 2>&1 || true
  done
  SILVER_OFFLINED=1
fi

TS="$(date +%Y%m%d_%H%M%S)"
TRACE_BASENAME="perfetto_${DEV}_t${N_THREADS}_cpuP${CPU_P}_ramP${RAM_P}_cpuD${CPU_D}_ramD${RAM_D}_${TS}.pftrace"
TRACE_OUT="$OUTPUT_DIR/$TRACE_BASENAME"

# Android traced는 output_path에 제약이 있을 수 있어서(/data/misc/perfetto-traces 권장),
# 거기에 먼저 저장 후 output/으로 복사.
# (TraceConfig 문서: output_path는 /data/misc/perfetto-traces/ 아래여야 실패하지 않음) :contentReference[oaicite:4]{index=4}
su -c "mkdir -p /data/misc/perfetto-traces" >/dev/null 2>&1 || true
TRACE_TMP="/data/misc/perfetto-traces/$TRACE_BASENAME"
TRACE_PUBLIC="/data/local/tmp/$TRACE_BASENAME"
PID_FILE="/data/local/tmp/perfetto_mllm_${TS}.pid"

CFG_LOCAL="$OUTPUT_DIR/perfetto_cfg_${TS}.pbtxt"
CFG_TMP="/data/local/tmp/perfetto_cfg_${TS}.pbtxt"

TRACE_LOG_TMP="/data/local/tmp/perfetto_${TS}.log"
TRACE_LOG_OUT="$OUTPUT_DIR/perfetto_${TS}.log"

# Only enable ftrace events that exist on this kernel.
event_exists() {
  local ev="$1"
  local cat="${ev%/*}"
  local name="${ev#*/}"
  [[ -d "$TRACEFS/events/$cat/$name" ]] && [[ -e "$TRACEFS/events/$cat/$name/enable" ]]
}

FTRACE_EVENTS=(
  # Per-core scheduling / thread placement (코어 점유율/활동률 기반)
  "sched/sched_switch"
  "sched/sched_wakeup"
  "sched/sched_wakeup_new"
  "sched/sched_waking"          # some kernels
  "sched/sched_migrate_task"
  "sched/sched_process_fork"
  "sched/sched_process_exit"
  "task/task_rename"
  "task/task_newtask"

  # CPU freq/idle (event-driven) + suspend
  "power/cpu_frequency"
  "power/cpu_idle"
  "power/suspend_resume"

  # Clock + interconnect (DDR/버스/“voter” 후보)
  "clk/clk_set_rate"
  "clk/clk_enable"
  "clk/clk_disable"
  "clk/clk_set_duty_cycle"
  "clk_qcom/clk_measure"
  "clk_qcom/clk_state"
  "interconnect/icc_set_bw"
  "interconnect/icc_set_bw_end"
  "interconnect_qcom/bcm_voter_commit"
  "samsung/tracing_mark_write"

  # dcvs
  "dcvs/bw_hwmon_update"
  "dcvs/bw_hwmon_meas"
  "dcvs/memlat_dev_meas"
  "dcvs/memlat_dev_update"
  "dcvs/qcom_dcvs_update"
  "dcvs/qcom_dcvs_boost"

  # devfreq tracepoints (있으면)
  "devfreq/devfreq_monitor"
  "devfreq/devfreq_frequency"

  # thermal tracepoints (있으면)
  "thermal/thermal_temperature"
  "thermal/cdev_update"
  "thermal/thermal_power_cpu_get_power_simple"
  "thermal/thermal_zone_trip"
  "thermal/thermal_power_devfreq_get_power"

  # optional memory event
  "kmem/rss_stat"
)

# Snapshot: devfreq/cpufreq 노드 매핑용 (S25에서 RAM/버스가 어떤 devfreq인지 찾을 때 도움)
{
  echo "# cpufreq snapshot"
  for p in /sys/devices/system/cpu/cpufreq/policy*; do
    [[ -d "$p" ]] || continue
    pol="$(basename "$p")"
    cur=""; [[ -f "$p/scaling_cur_freq" ]] && cur="$(cat "$p/scaling_cur_freq" 2>/dev/null)"
    gov=""; [[ -f "$p/scaling_governor" ]] && gov="$(cat "$p/scaling_governor" 2>/dev/null)"
    echo "$pol cur_freq=$cur governor=$gov"
  done

  echo
  echo "# devfreq snapshot"
  for d in /sys/class/devfreq/*; do
    [[ -d "$d" ]] || continue
    name="$(basename "$d")"
    cur=""; [[ -f "$d/cur_freq" ]] && cur="$(cat "$d/cur_freq" 2>/dev/null)"
    gov=""; [[ -f "$d/governor" ]] && gov="$(cat "$d/governor" 2>/dev/null)"
    avail=""; [[ -f "$d/available_frequencies" ]] && avail="$(cat "$d/available_frequencies" 2>/dev/null)"
    echo "$name cur_freq=$cur governor=$gov avail=[${avail}]"
  done
} > "$OUTPUT_DIR/freq_snapshot_${TS}.txt" 2>/dev/null || true

# Build Perfetto textproto config.
# - sched_switch 기반으로 코어별 점유/스레드 배치 확인
# - sys_stats: meminfo/vmstat + cpufreq/devfreq + thermal + PSI
#   (cpufreq_period_ms/devfreq_period_ms는 sys_stats_config의 필드로 정의됨) :contentReference[oaicite:5]{index=5}
# - process_stats: 프로세스 RSS/virt/smaps_rollup 등 폴링 :contentReference[oaicite:6]{index=6}
cat > "$CFG_LOCAL" <<CONFIG_HEAD
buffers { size_kb: 65536 fill_policy: RING_BUFFER }

data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      # Reduce size for sched events.
      compact_sched { enabled: true }
CONFIG_HEAD

for ev in "${FTRACE_EVENTS[@]}"; do
  if event_exists "$ev"; then
    echo "      ftrace_events: \"$ev\"" >> "$CFG_LOCAL"
  fi
done

cat >> "$CFG_LOCAL" <<'CONFIG_TAIL'
    }
  }
}

data_sources {
  config {
    name: "linux.sys_stats"
    sys_stats_config {
      # System RAM + VM counters
      meminfo_period_ms: 1000
      vmstat_period_ms: 1000

      # Per-CPU current freq + devfreq current freq (RAM/bus candidates)
      # Note: polling periods inside sys_stats_config must be integer multiples.
      cpufreq_period_ms: 100
      devfreq_period_ms: 100

      # Thermal zones + PSI (CPU/memory/IO pressure)
      thermal_period_ms: 1000
      psi_period_ms: 1000
    }
  }
}

data_sources {
  config {
    name: "linux.process_stats"
    process_stats_config {
      scan_all_processes_on_start: true
      proc_stats_poll_ms: 1000
      record_process_runtime: true
      scan_smaps_rollup: true
    }
  }
}

data_sources {
  config { name: "linux.system_info" }
}
CONFIG_TAIL

# Make config readable by root + perfetto.
su -c "cp '$CFG_LOCAL' '$CFG_TMP' && chmod 644 '$CFG_TMP'" >/dev/null 2>&1 || true

stop_perfetto() {
  local pid
  pid="$(su -c "cat '$PID_FILE'" 2>/dev/null || true)"
  if [[ -n "$pid" ]]; then
    su -c "kill -INT $pid" >/dev/null 2>&1 || true
    # Best-effort wait a bit for flush.
    for _ in {1..50}; do
      su -c "kill -0 $pid" >/dev/null 2>&1 || break
      sleep 0.1
    done
  fi
}

restore_settings() {
  # Bring back silver cores if we turned them off.
  if [[ "$SILVER_OFFLINED" -eq 1 ]]; then
    for c in 1 2 3; do
      su -c "echo 1 > /sys/devices/system/cpu/cpu${c}/online" >/dev/null 2>&1 || true
    done
  fi

  # Restore governor (vendor-specific; keep best-effort).
  su -c "echo walt > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor" >/dev/null 2>&1 || true
  su -c "echo walt > /sys/devices/system/cpu/cpufreq/policy6/scaling_governor" >/dev/null 2>&1 || true

  # Turn screen back on.
  if [[ "$DEV" == "Pixel9" || "$DEV" == "S24" || "$DEV" == "S25" ]]; then
    su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness" >/dev/null 2>&1 || true
  else
    su -c "echo 3310 > /sys/class/backlight/panel/brightness" >/dev/null 2>&1 || true
  fi
}

cleanup() {
  stop_perfetto || true
  restore_settings || true
}
trap cleanup EXIT INT TERM

# Start Perfetto trace (root).
su -c "rm -f '$TRACE_TMP' '$TRACE_PUBLIC' '$PID_FILE' '$TRACE_LOG_TMP'" >/dev/null 2>&1 || true
su -c "$PERFETTO_BIN --txt -c '$CFG_TMP' -o '$TRACE_TMP' >'$TRACE_LOG_TMP' 2>&1 & echo \$! > '$PID_FILE'" >/dev/null 2>&1
sleep 0.2

# Add markers to ftrace (optional).
# ftrace 이벤트 sysfs는 /sys/kernel/tracing 아래에 있음 :contentReference[oaicite:7]{index=7}
if [[ -n "$TRACEFS" && -e "$TRACEFS/trace_marker" ]]; then
  su -c "echo 'MLLM_RUN_BEGIN dev=$DEV t=$N_THREADS cpuP=$CPU_P ramP=$RAM_P cpuD=$CPU_D ramD=$RAM_D ts=$TS' > '$TRACEFS/trace_marker'" >/dev/null 2>&1 || true
fi

# -------------------
# Run the CPU model
# -------------------
./bin-arm/stream_qwen \
  -m models/qwen-1-5-0.5b-q4_k.mllm \
  -v vocab/qwen_vocab.mllm \
  -e vocab/qwen_merges.txt \
  -f Qwen1.5 \
  -b 0.5B \
  -t "$N_THREADS" \
  -l 1024 \
  -i 1 \
  -s 1 \
  -L 30 \
  -I dataset/hotpot_qa.csv \
  -O output/ \
  -S 0 \
  -D "$DEV" \
  --strict 0 \
  --cpu-p "$CPU_P" --ram-p "$RAM_P" \
  --cpu-d "$CPU_D" --ram-d "$RAM_D" \
  --phase-pause 0 \
  --token-pause 0 \
  --layer-pause 0 \
  --query-interval 0

if [[ -n "$TRACEFS" && -e "$TRACEFS/trace_marker" ]]; then
  su -c "echo 'MLLM_RUN_END dev=$DEV ts=$TS' > '$TRACEFS/trace_marker'" >/dev/null 2>&1 || true
fi

# Stop perfetto and copy results into output/.
stop_perfetto
su -c "chmod 644 '$TRACE_TMP' '$TRACE_LOG_TMP'" >/dev/null 2>&1 || true
su -c "cp '$TRACE_TMP' '$TRACE_PUBLIC' && chmod 644 '$TRACE_PUBLIC'" >/dev/null 2>&1 || true

cp -f "$TRACE_PUBLIC" "$TRACE_OUT" || true
cp -f "$TRACE_LOG_TMP" "$TRACE_LOG_OUT" || true

echo "Perfetto trace saved: $TRACE_OUT"
echo "Perfetto log saved:   $TRACE_LOG_OUT"
echo "Backup copy (adb-friendly): $TRACE_PUBLIC"