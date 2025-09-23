# turn-off screen
# Pixel9-only
su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"

# silver core control
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./bin-arm/stream_qwen3 \
  -m models/qwen3-1.7b-q4_k.mllm \
  -v vocab/qwen3_vocab.mllm \
  -e vocab/qwen3_merges.txt \
  -b 1.7B \
  -t 4 \
  -l 1024 \
  -i 1 \
  -s 1 \
  -L 20 \
  -I dataset/hotpot_qa.csv \
  -O output/ \
  -S 0 \
  -D Pixel9 \
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

# turn-on screen
# Pixel9-only
su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"