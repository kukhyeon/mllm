# turn-off screen
# Pixel9-only
su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"

# silver core control
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./bin-arm/stream_qwen \
  -m models/qwen-1.5-1.8b-q4_k.mllm \
  -v vocab/qwen_vocab.mllm \
  -e vocab/qwen_merges.txt \
  -b 1.8B \
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
  --token-pause 0

# [pause-unit] = ms

# turn-on screen
# Pixel9-only
su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"