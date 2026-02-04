#!/system/bin/sh

echo "--- Selected Device Temperatures ---"
printf "%-30s %s\n" "[Sensor Type]" "[Temp]"
echo "----------------------------------------------"

for i in /sys/class/thermal/thermal_zone*; do
    if [ -d "$i" ]; then
        TYPE=$(cat "$i/type")
        
        # Filltering
        case "$TYPE" in
            cpu*|nsphvx*|pm*|battery*|sys-therm*)
                TEMP_RAW=$(cat "$i/temp")
                TEMP_CELSIUS=$((TEMP_RAW / 1000))
                if [ "$TEMP_CELSIUS" -gt -50 ]; then
                    printf "%-30s %3d°C\n" "$TYPE" "$TEMP_CELSIUS"
                fi
                ;;
            *)
                continue
                ;;
        esac
    fi
done