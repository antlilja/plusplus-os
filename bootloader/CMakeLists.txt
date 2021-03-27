add_executable(bootloader
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/elf-loader.c
)

target_include_directories(bootloader PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/efilib/include
  ${PROJECT_SOURCE_DIR}/efilib/include/x86_64
)

target_compile_options(bootloader PRIVATE
  -target x86_64-unknown-windows
  -ffreestanding
  -fshort-wchar
  -mno-red-zone
  -O2
)

target_link_options(bootloader PRIVATE
  -target x86_64-unknown-windows
  -nostdlib
  -Wl,-entry:efi_main
  -Wl,-subsystem:efi_application
  -fuse-ld=lld-link
)

target_compile_features(bootloader PRIVATE c_std_11)
