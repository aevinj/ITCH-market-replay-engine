add_executable(OrderBookApp
    main.cpp
    TerminalDashboard.cpp
)

target_include_directories(OrderBookApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(OrderBookApp
    PRIVATE
        matching_engine
        orderbook
)
