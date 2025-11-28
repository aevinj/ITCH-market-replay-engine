# ---- GoogleTest setup ----
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(OrderBookTests OrderBookTests.cpp)
target_link_libraries(OrderBookTests PRIVATE orderbook gtest_main)

include(GoogleTest)
gtest_discover_tests(OrderBookTests)
