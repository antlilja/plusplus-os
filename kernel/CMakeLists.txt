add_executable(kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gdt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/paging.c
)

set_target_properties(kernel PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

target_include_directories(kernel PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(kernel PRIVATE
  -target x86_64-pc-none-elf
  -ffreestanding
  -mno-red-zone
  -fno-builtin
  -fPIE
  -O2
)


target_link_options(kernel PRIVATE
  -target x86_64-pc-none-elf
  -ffreestanding
  -nostdlib
  -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
)

target_compile_features(kernel PRIVATE c_std_11)
