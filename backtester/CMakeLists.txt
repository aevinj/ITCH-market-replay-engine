add_library(backtester
    Backtester.cpp
)

target_include_directories(backtester PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(backtester PUBLIC matching_engine orderbook)
