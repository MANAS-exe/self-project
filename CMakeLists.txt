cmake_minimum_required(VERSION 3.25)
project(PortfolioManager VERSION 1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Libraries directory (MSYS2 UCRT64)
link_directories(/ucrt64/lib)

# Add executable
add_executable(PortfolioManager
    src/main.cpp
    src/Database.cpp
    src/ApiClient.cpp
    src/Portfolio.cpp
    src/CLI.cpp
)

# Link libraries
target_link_libraries(PortfolioManager
    curl
    sqlite3
    jsoncpp
)
