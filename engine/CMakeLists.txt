add_library(matching_engine
    MatchingEngine.cpp
)

target_include_directories(matching_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(matching_engine PUBLIC orderbook)
