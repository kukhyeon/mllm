#!/system/bin/sh

echo "[*] Disabling SELinux Enforcing..."
setenforce 0

echo "[*] Disabling ZRAM Swap..."
swapoff /dev/block/zram0

echo "[*] Dropping Caches (Memory Optimization)..."
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[*] Stopping Thermal Daemons..."
stop thermal-engine
stop vendor.samsung.hardware.thermal-default

echo "[*] Checking Thermal Properties..."
getprop | grep thermal

echo "[*] Disabling Samsung GOS & Game Tools..."
pm disable-user com.samsung.android.game.gos
pm disable-user com.samsung.android.game.gametools
pm disable-user com.samsung.android.game.gamehome

echo "[!] Performance optimization applied."