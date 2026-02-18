#!/system/bin/sh

echo "[*] Disabling Samsung GOS & Game Tools..."
pm disable-user com.samsung.android.game.gos
pm disable-user com.samsung.android.game.gametools
pm disable-user com.samsung.android.game.gamehome

echo "[!] Performance optimization applied."