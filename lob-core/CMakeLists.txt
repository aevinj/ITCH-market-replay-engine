add_library(orderbook
    LimitOrderBook.cpp
)

target_include_directories(orderbook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    target_compile_options(orderbook PRIVATE /W4 /permissive-)
else()
    target_compile_options(orderbook PRIVATE -Wall -Wextra -Wpedantic)
endif()
