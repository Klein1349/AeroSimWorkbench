# The main application and both acceptance probes resolve the same production
# targets in this build graph. This hook is used by the isolated build only.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_BINARY_DIR}/acceptance" EXCLUDE_FROM_ALL)
