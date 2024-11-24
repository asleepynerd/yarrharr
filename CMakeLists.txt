cmake_minimum_required(VERSION 3.15)
project(yarrharr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/downloader.cpp
    src/tmdb.cpp
    src/utils.cpp
    src/config.cpp
    src/version.cpp
)

# Create executable
add_executable(yarrharr ${SOURCES})

# Include directories
target_include_directories(yarrharr PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(yarrharr PRIVATE 
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Windows-specific configurations
if(WIN32)
    target_compile_definitions(yarrharr PRIVATE _WIN32_WINNT=0x0601)
endif()

# Installation rules
install(TARGETS yarrharr DESTINATION bin) 