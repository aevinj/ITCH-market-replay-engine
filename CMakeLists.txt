cmake_minimum_required(VERSION 3.25)

project(HighPerformanceOrderBook
    VERSION 0.2.0
    DESCRIPTION "LOB + Matching Engine + Backtester"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(lob-core)
add_subdirectory(engine)
add_subdirectory(backtester)
add_subdirectory(app)
add_subdirectory(tests)
