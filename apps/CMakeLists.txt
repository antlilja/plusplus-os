add_subdirectory(terminal)
add_subdirectory(sortex)

add_custom_target(apps
  DEPENDS terminal
  DEPENDS sortex
  VERBATIM)
