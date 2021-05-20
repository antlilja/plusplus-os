add_subdirectory(terminal)

add_custom_target(apps
  DEPENDS terminal
  VERBATIM)
