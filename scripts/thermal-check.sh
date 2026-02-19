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

echo "--- System Thermal Status ---"
# dumpsys에서 Thermal Status: X 값을 추출
STATUS_VAL=$(dumpsys thermalservice | grep "Thermal Status:" | awk -F': ' '{print $2}' | head -n 1)

if [ -n "$STATUS_VAL" ]; then
    case "$STATUS_VAL" in
        0) DESC="NONE (Normal)" ;;
        1) DESC="LIGHT" ;;
        2) DESC="MODERATE" ;;
        3) DESC="SEVERE" ;;
        4) DESC="CRITICAL" ;;
        5) DESC="EMERGENCY" ;;
        6) DESC="SHUTDOWN" ;;
        *) DESC="UNKNOWN" ;;
    esac
    echo "Current Thermal Status: $STATUS_VAL ($DESC)"
else
    echo "Thermal Status: Could not be retrieved (Check root/permission)"
fi
