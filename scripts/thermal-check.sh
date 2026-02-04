#!/system/bin/sh

echo "--- Current Device Temperatures ---"
for i in /sys/class/thermal/thermal_zone*; do
    # Check Path
    if [ -d "$i" ]; then
        TYPE=$(cat "$i/type")
        TEMP_RAW=$(cat "$i/temp")
        # Range Regularization
        TEMP_CELSIUS=$((TEMP_RAW / 1000))
        printf "%-30s %3d°C\n" "$TYPE" "$TEMP_CELSIUS"
    fi
done