# product name
DEV="$(getprop ro.product.product.model)"
DEV="$(printf '%s' "$DEV" | tr -d '[:space:]')"
echo "Device: $DEV"

# turn-off screen
if [ "$DEV" = "Pixel9" ]; then
  # Pixel9
  su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"
elif [ "$DEV" = "S24" ] || [ "$DEV" = "S25" ]; then
  # S24, S25
  su -c "echo 0 > /sys/class/backlight/panel/brightness"
else
  # Default 
  su -c "echo 0 > /sys/class/backlight/panel/brightness"
  DEV="S24"
fi

sleep 3 # stabilize

# silver core control
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./bin-arm/stream_qwen3 \
  # -m: model path
  # -v: vocabulary path
  # -e: merges path
  # -f: model family
  # -b: model size
  # -t: num of threads
  # -l: max KV cache size
  # -i: print inference interface
  # -s: starting num of queries
  # -L: num of queries

  # -I: input dataset path of csv
  # -O: output directory path
  # -S: save query-answer pairs with json
  # -D: device name
  # --strict: apply tokwn limits to only output tokens
  # --cpu-p: specify CPU frequency for CPU DVFS
  # --ram-p: specify RAM frequency for RAM DVFS
  # --cpu-d: specify CPU frequency for CPU DVFS
  # --ram-d: specify RAM frequency for RAM DVFS
  # --phase-pause: specify a pause time between phases (ms)
  # --token-pause: specify a pause time between generation tokens (ms)
  # --layer-pause: specify a pause time between self-attention layers during prefill (ms)
  # --query-interval: specify an interval time between queries (s)
  -m models/qwen-3-1.7b-q4k.mllm \
  -v vocab/qwen3_vocab.mllm \
  -e vocab/qwen3_merges.txt \
  -f Qwen1.5 \
  -b 1.7B \
  -t 4 \
  -l 1024 \
  -i 1 \
  -s 1 \
  -L 20 \

  -I dataset/hotpot_qa.csv \
  -O output/ \
  -S 0 \
  -D "$DEV" \
  --strict 0 \
  --cpu-p $1 \
  --ram-p $2 \
  --cpu-d $3 \
  --ram-d $4 \
  --phase-pause 0 \
  --token-pause 0 \
  --layer-pause 0 \
  --query-interval 0

# [pause-unit] = ms
# [interval-unit] = s

# silver core reset
su -c "echo 1 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 1 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 1 > /sys/devices/system/cpu/cpu3/online"

# turn-on screen
if [ "$DEV" = "Pixel9" ]; then
  # Pixel9
  su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"
else
  # S24, S25
  su -c "echo 1023 > /sys/class/backlight/panel/brightness"
fi
