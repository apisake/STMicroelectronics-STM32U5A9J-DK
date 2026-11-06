#!/bin/bash
# Build GPIO_IOToggle (+LVGL) with Arm GCC, parallel.
# Usage: tools/build.sh <outdir> [optlevel]      NEOCHROM=1 tools/build.sh <outdir>  -> GPU2D renderer
D="$(cd "$(dirname "$0")/.." && pwd)"; P="$D/GPIO_IOToggle"; O="$1"; OPT="${2:--O2}"
mkdir -p "$O/obj"; cd "$O" || exit 1
export PATH=~/Library/Arduino15/packages/SiliconLabs/tools/gcc-arm-none-eabi/12.2.rel1/bin:$PATH
ARCH="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"

# sources: single-file links from .project + every .c in the linked lvgl folder
{
  grep -o "PARENT-[12]-PROJECT_LOC/[^<]*\.c" "$P/STM32CubeIDE/.project" | sed -e "s#PARENT-1-PROJECT_LOC#$P#" -e "s#PARENT-2-PROJECT_LOC#$D#"
  find "$D/Middlewares/lvgl/src" -name '*.c'
  echo "$P/STM32CubeIDE/Application/User/syscalls.c"
  echo "$P/STM32CubeIDE/Application/User/sysmem.c"
} > srcs.txt

cat > cflags.txt <<EOF
$ARCH $OPT -g3 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32U5A9xx -DLV_CONF_INCLUDE_SIMPLE ${NEOCHROM:+-DUSE_NEOCHROM}
-ffunction-sections -fdata-sections -Wall
-I"$P/Inc" -I"$D/Drivers/STM32U5xx_HAL_Driver/Inc" -I"$D/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy"
-I"$D/Drivers/CMSIS/Device/ST/STM32U5xx/Include" -I"$D/Drivers/CMSIS/Include"
-I"$D/Drivers/BSP/STM32U5x9J-DK" -I"$D/Drivers/BSP/Components/Common" -I"$D/Utilities/lcd"
-I"$D/Middlewares/lvgl"
EOF

compile() {   # $1 = source path
  local rel="${1#$D/}"; local obj="obj/$(echo "$rel" | tr '/ ' '__').o"
  if [ ! -f "$obj" ] || [ "$1" -nt "$obj" ] || [ cflags.txt -nt "$obj" ] || [ "$D/GPIO_IOToggle/Inc/lv_conf.h" -nt "$obj" ]; then
    arm-none-eabi-gcc @cflags.txt -c "$1" -o "$obj" 2>>compile.log || { echo "FAIL $1" >> compile.log; return 1; }
  fi
}
export -f compile; export D
: > compile.log; : > link.log
tr '\n' '\0' < srcs.txt | xargs -0 -n1 -P8 bash -c 'compile "$0"'
arm-none-eabi-gcc $ARCH -c "$P/STM32CubeIDE/Application/Startup/startup_stm32u5a9njhxq.s" -o obj/startup.o
arm-none-eabi-gcc $ARCH -T"$P/STM32CubeIDE/STM32U5A9NJHXQ_FLASH.ld" --specs=nano.specs -Wl,--gc-sections \
  -Wl,-Map=out.map obj/*.o ${NEOCHROM:+"$D/Middlewares/lvgl/libs/nema_gfx/lib/core/cortex_m33_revC/gcc/libnemagfx-float-abi-hard.a"} -Wl,--start-group -lc -lm -Wl,--end-group -o GPIO_IOToggle.elf 2>>link.log
echo "sources=$(wc -l < srcs.txt) fails=$(grep -c '^FAIL' compile.log) warnings=$(grep -c 'warning:' compile.log)"
arm-none-eabi-size GPIO_IOToggle.elf
