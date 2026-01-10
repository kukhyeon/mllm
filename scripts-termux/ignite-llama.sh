# product name
# DEV="$(getprop ro.product.product.model)"
# DEV="$(printf '%s' "$DEV" | tr -d '[:space:]')"
DEV="S25"
echo "Device: $DEV"

# turn-off screen
if [ "$DEV" = "S25" ]; then
  # Pixel9, S25
  su -c "echo 0 > /sys/class/backlight/panel0-backlight/brightness"
elif [ "$DEV" = "S24" ] || [ "$DEV" = "S25" ]; then
  # S24
  su -c "echo 0 > /sys/class/backlight/panel/brightness"
else
  # Default 
  su -c "echo 0 > /sys/class/backlight/panel/brightness"
  DEV="S24"
fi

sleep 3 # stabilize

# CPU Governor: performance
su -c "echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
su -c "echo performance > /sys/devices/system/cpu/cpufreq/policy6/scaling_governor"
echo "CPU Governor (policy0): $(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor)"
echo "CPU Governor (policy6): $(cat /sys/devices/system/cpu/cpufreq/policy6/scaling_governor)"
sleep 1

# silver core control (Except S25)
if [ "$DEV" != "S25" ]; then
  su -c "echo 0 > /sys/devices/system/cpu/cpu1/online"
  su -c "echo 0 > /sys/devices/system/cpu/cpu2/online"
  su -c "echo 0 > /sys/devices/system/cpu/cpu3/online"
fi

# drop caches
su -c "sync"
echo 3 | su -c "tee /proc/sys/vm/drop_caches"
sleep 3

./bin-arm/stream_llama3 \
  -m models/llama3.2-3b-q4k.mllm \
  -v vocab/llama3_tokenizer.model \
  -b 3b \
  -t 8 \
  -l 1024 \
  -i 1 \
  -s 1 \
  -L 4 \
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

# silver core reset (except S25)
if [ "$DEV" != "S25" ]; then
  su -c "echo 1 > /sys/devices/system/cpu/cpu1/online"
  su -c "echo 1 > /sys/devices/system/cpu/cpu2/online"
  su -c "echo 1 > /sys/devices/system/cpu/cpu3/online"
fi

# CPU Governor reset: walt
su -c "echo walt > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
su -c "echo walt > /sys/devices/system/cpu/cpufreq/policy6/scaling_governor"
echo "CPU Governor reset (policy0): $(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_governor)"
echo "CPU Governor reset (policy6): $(cat /sys/devices/system/cpu/cpufreq/policy6/scaling_governor)"

# turn-on screen
if [ "$DEV" = "S25" ]; then
  # S25
  su -c "echo 1023 > /sys/class/backlight/panel0-backlight/brightness"
else
  # S24, S25
  su -c "echo 1023 > /sys/class/backlight/panel/brightness"
fi
