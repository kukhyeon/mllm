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

# --- 추가: Cooling Device 정보 ---
echo ""
echo "--- Cooling Device States ---"
printf "%-30s %s\n" "[Cooling Device]" "[Cur/Max]"
echo "----------------------------------------------"
for i in /sys/class/thermal/cooling_device*; do
    if [ -d "$i" ]; then
        TYPE=$(cat "$i/type")
        CUR_STATE=$(cat "$i/cur_state")
        MAX_STATE=$(cat "$i/max_state")
        
        # 모든 쿨링 디바이스 출력 (상태가 0보다 큰 것만 필터링하고 싶으면 조건 추가 가능)
        printf "%-30s %d/%d\n" "$TYPE" "$CUR_STATE" "$MAX_STATE"
    fi
done

# --- 추가: Android Thermal Status 파싱 ---
echo ""
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