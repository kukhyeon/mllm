
# turn-off screen
# Pixel9-only
su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"

# silver core control
su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"

./bin-arm/stream_qwen \
  -m models/qwen-1.5-0.5b-q4_k.mllm \
  -v vocab/qwen_vocab.mllm \
  -e vocab/qwen_merges.txt \
  -b 0.5B \
  -t 4 \
  -l 1024 \
  -i 1 \
  -s 1 \
  -L 20 \
  -I dataset/hotpot_qa.csv \
  -O output/ \
  -S 0 \
  -D Pixel9 \
  --cpu $1 \
  --ram $2 


# [pause-unit] = ms

# turn-on screen
# Pixel9-only
su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"