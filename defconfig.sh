#!/bin/bash

actual_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

KERNEL_SOURCE_DIR=/home/sajid/KERNEL/android_kernel_xiaomi_sdm845

cd ${KERNEL_SOURCE_DIR}
ARCH=arm64 make defconfig KBUILD_DEFCONFIG=noto_defconfig
mv .config ${KERNEL_SOURCE_DIR}/arch/arm64/configs/noto_defconfig

ARCH=arm64 make defconfig KBUILD_DEFCONFIG=beryllium_stock_defconfig
mv .config ${KERNEL_SOURCE_DIR}/arch/arm64/configs/beryllium_stock_defconfig

clear
git add arch/arm64/configs/beryllium_defconfig
git commit -am "arch/arm64: Sync beryllium defconfig"
echo "Dont forget to run git commit --amend -s"
