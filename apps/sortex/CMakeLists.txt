add_executable(sortex
  src/rendering.c
  src/main.c
)

set_target_properties(sortex PROPERTIES LINK_DEPENDS ${PROJECT_SOURCE_DIR}/apps/linker.ld)

target_include_directories(sortex PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/include

)

target_compile_options(sortex PRIVATE
  # -O2
  -fPIE
  -Wall
  -Wextra
)

target_link_options(sortex PRIVATE
  -T ${PROJECT_SOURCE_DIR}/apps/linker.ld
  -nostdlib
  -Wl,--build-id=none
  -ffreestanding
)
