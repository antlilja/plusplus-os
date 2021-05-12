option(ENABLE_KERNEL_ASSERTS "Enables asserts in the kernel" ON)

add_executable(kernel
  ${CMAKE_CURRENT_SOURCE_DIR}/src/stage1_entry.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gdt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/idt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/port_io.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/acpi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pci.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/apic.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ahci.c

  # Memory
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/paging.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/frame_allocator.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/memory/entry_pool.c

  # Libc kernel sources
  ${PROJECT_SOURCE_DIR}/libc/src/string/memcpy.c
  ${PROJECT_SOURCE_DIR}/libc/src/string/memmove.c
  ${PROJECT_SOURCE_DIR}/libc/src/string/memset.c
  ${PROJECT_SOURCE_DIR}/libc/src/string/memcmp.c
  ${PROJECT_SOURCE_DIR}/libc/src/string/strcmp.c
)

set_target_properties(kernel PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

target_include_directories(kernel PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/include

  # Libc kernel include
  ${PROJECT_SOURCE_DIR}/libc/include
)

target_compile_options(kernel PRIVATE
  -target x86_64-pc-none-elf
  -ffreestanding
  -mno-red-zone
  -fno-builtin
  -fPIE
  -O2
  -Wall
  -Wextra
)

target_link_options(kernel PRIVATE
  -target x86_64-pc-none-elf
  -ffreestanding
  -nostdlib
  -Wl,--build-id=none
  -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
)

if(ENABLE_KERNEL_ASSERTS)
  target_compile_definitions(kernel PRIVATE
    ENABLE_KERNEL_ASSERTS
  )
endif()

target_compile_features(kernel PRIVATE c_std_11)
