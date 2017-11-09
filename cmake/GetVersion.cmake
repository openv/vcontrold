# Get version-string lookalike from git
execute_process(
  COMMAND git describe --always --tags --dirty --long
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_CODE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VERSION "${GIT_CODE_VERSION}")
message(STATUS "Version string: ${GIT_CODE_VERSION}")
string(TIMESTAMP BUILD_TIMESTAMP %Y%m%d%H%M%S UTC)
add_definitions(-DGIT_CODE_VERSION="${GIT_CODE_VERSION}" -DBUILD_TIMESTAMP="${BUILD_TIMESTAMP}")

