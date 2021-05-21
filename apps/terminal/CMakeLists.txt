add_executable(terminal
  src/main.c
)

set_target_properties(terminal PROPERTIES LINK_DEPENDS ${PROJECT_SOURCE_DIR}/apps/linker.ld)

target_compile_options(terminal PRIVATE
  -O2
  -fPIE
  -Wall
  -Wextra
)

target_link_options(terminal PRIVATE
  -T ${PROJECT_SOURCE_DIR}/apps/linker.ld
  -nostdlib
  -Wl,--build-id=none
  -ffreestanding
)
