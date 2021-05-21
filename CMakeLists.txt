cmake_minimum_required(VERSION 3.2)

project(
  plusplus-os
  VERSION 1.0.0
  LANGUAGES C
)

set(CMAKE_C_COMPILER clang)

set(QEMU_ARGS "" CACHE STRING "QEMU extra args")
set(UEFI_FIRMWARE "" CACHE STRING "UEFI firmware path")

add_subdirectory(bootloader)
add_subdirectory(kernel)
add_subdirectory(apps)

add_custom_target(image
  DEPENDS bootloader
  DEPENDS kernel
  DEPENDS apps
  COMMAND dd if=/dev/zero of=${CMAKE_BINARY_DIR}/boot.img bs=1k count=1440
  COMMAND mformat -i boot.img -f 1440 ::
  COMMAND mmd -i boot.img ::/EFI
  COMMAND mmd -i boot.img ::/EFI/BOOT
  COMMAND mcopy -i boot.img ${CMAKE_BINARY_DIR}/bootloader/bootloader ::/EFI/BOOT/BOOTX64.EFI
  COMMAND mcopy -i boot.img ${CMAKE_BINARY_DIR}/kernel/kernel ::/KERNEL.ELF
  COMMAND mcopy -i boot.img ${CMAKE_BINARY_DIR}/apps/terminal/terminal ::/TERMINAL
  COMMAND mcopy -i boot.img ${CMAKE_BINARY_DIR}/apps/sortex/sortex ::/SORTEX
  VERBATIM)

add_custom_target(run
  COMMAND qemu-system-x86_64 ${QEMU_ARGS} -machine q35 -bios ${UEFI_FIRMWARE} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img
  DEPENDS image
  VERBATIM)
