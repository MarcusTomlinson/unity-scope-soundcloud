
# Put the ini file in the build directory next to the scope
# .so file so test tools can find both easily.
intltool_merge_translations(
  "${CMAKE_SOURCE_DIR}/data/${SCOPE_NAME}.ini.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${SCOPE_NAME}.ini"
  ALL
  UTF8
)

function(configure_icons)
  foreach(_file ${ARGV})
    configure_file(
      "${CMAKE_SOURCE_DIR}/data/${_file}"
      "${CMAKE_CURRENT_BINARY_DIR}/${_file}"
      @ONLY
      COPYONLY
    )
  endforeach()
endfunction()

configure_icons(
  "comments.png"
  "likes.png"
  "logo.png"
  "plays.png"
  "reposts.png"
)

# The sources to build the scope
set(SCOPE_SOURCES
  api/client.cpp
  api/track.cpp
  api/user.cpp
  api/comment.cpp
  scope/preview.cpp
  scope/query.cpp
  scope/scope.cpp
  scope/activation.cpp
)

# Find all the headers
file(GLOB_RECURSE
  SCOPE_HEADERS
  "${CMAKE_SOURCE_DIR}/include/*.h" 
)

# Build an object library for the scope code
add_library(
  scope-static OBJECT
  ${SCOPE_SOURCES}
  ${SCOPE_HEADERS}
)

# Ensure we export all the symbols
set_target_properties(
  scope-static
  PROPERTIES
    LINK_FLAGS "-Wl,--export-all-symbols"
)

# Build a shared library containing our scope code.
# This will be the actual plugin that is loaded.
add_library(
  scope SHARED
  $<TARGET_OBJECTS:scope-static>
)

# Link against the object library and our external library dependencies
target_link_libraries(
  scope
  ${SCOPE_LDFLAGS}
  ${Boost_LIBRARIES}
)


# Set the correct library output name to conform to the securiry policy 
set_target_properties(
  scope
  PROPERTIES
    OUTPUT_NAME "com.ubuntu.scopes.soundcloud_soundcloud"
)

# Install the scope shared library
install(
  TARGETS scope
  LIBRARY DESTINATION ${SCOPE_INSTALL_DIR}
)

