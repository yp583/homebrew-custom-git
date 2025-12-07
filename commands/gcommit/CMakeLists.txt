cmake_minimum_required(VERSION 3.14)
project(git_gcommit)

# Suppress deprecated FetchContent_Populate warnings from dependencies
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# Export compile_commands.json for clangd/IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug build with Address Sanitizer
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# Download CPM.cmake
set(CPM_DOWNLOAD_VERSION 0.42.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Add cpp-tree-sitter
CPMAddPackage(
  NAME cpp-tree-sitter
  GIT_REPOSITORY https://github.com/nsumner/cpp-tree-sitter.git
  GIT_TAG v0.0.2
)

# Add the Python grammar (fix the version specification)
add_grammar_from_repo(tree-sitter-python
  https://github.com/tree-sitter/tree-sitter-python.git
  0.20.4  # Remove the 'v' prefix entirely
)

add_grammar_from_repo(tree-sitter-cpp
  https://github.com/tree-sitter/tree-sitter-cpp.git
  0.23.4  # Remove the 'v' prefix entirely
)

add_grammar_from_repo(tree-sitter-java
  https://github.com/tree-sitter/tree-sitter-java.git
  0.23.0
)

add_grammar_from_repo(tree-sitter-javascript
  https://github.com/tree-sitter/tree-sitter-javascript.git
  0.23.0
)

# TypeScript support temporarily disabled due to build issues
# add_grammar_from_repo(tree-sitter-typescript
#   https://github.com/tree-sitter/tree-sitter-typescript.git
#   0.20.4
# )

add_grammar_from_repo(tree-sitter-go
  https://github.com/tree-sitter/tree-sitter-go.git
  0.23.0
)

# Add nlohmann/json
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.11.3
  GITHUB_REPOSITORY nlohmann/json
)

# Add umappp for UMAP dimensionality reduction
CPMAddPackage(
  NAME umappp
  GIT_REPOSITORY https://github.com/libscran/umappp
  GIT_TAG master
  OPTIONS "UMAPPP_FETCH_EXTERN ON"
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Create shared library from shared source files
add_library(custom_git_shared STATIC
    ../../shared/ast.cpp
    ../../shared/https_api.cpp
    ../../shared/openai_api.cpp
    ../../shared/async_https_api.cpp
    ../../shared/async_openai_api.cpp
    ../../shared/utils.cpp
    ../../shared/diffreader.cpp
)

# Set up include directories for shared library
target_include_directories(custom_git_shared 
    PUBLIC 
        ../../shared
        ${cpp-tree-sitter_SOURCE_DIR}/include
)

# Link dependencies to shared library
target_link_libraries(custom_git_shared
    PUBLIC
        cpp-tree-sitter
        tree-sitter-python
        tree-sitter-cpp
        tree-sitter-java
        tree-sitter-javascript
        tree-sitter-go
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Create the main executable
add_executable(git_gcommit.o
    src/main.cpp
    src/hierarchal.cpp
    src/kmeans.cpp
)

# Set up include directories for executable
target_include_directories(git_gcommit.o PRIVATE
    src
    ../../shared
)

# Link the executable to the shared library
target_link_libraries(git_gcommit.o
    custom_git_shared
    libscran::umappp
) 