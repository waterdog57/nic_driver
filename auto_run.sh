#! /bin/bash

MODULE_NAME="r8139"

sudo dmesg -C

echo "=== 1. 嘗試停用介面並卸載模組 ==="
sudo rmmod $MODULE_NAME 2>/dev/null

if [ $? -eq 0 ]; then
    echo "成功卸載 $MODULE_NAME"
else
    echo "模組未載入或卸載失敗（可能是首次載入）"
fi

echo "=== 3. 重新載入模組 ==="
sudo insmod ${MODULE_NAME}.ko

if [ $? -eq 0 ]; then
    echo "成功載入 ${MODULE_NAME}.ko！"
    echo "=== 4. 查看最新 10 行 dmesg ==="
    dmesg | tail -n 10
else
    echo "載入失敗，請檢查 dmesg！"
fi