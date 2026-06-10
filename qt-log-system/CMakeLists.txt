cmake_minimum_required(VERSION 3.16)
project(QtLogSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# ============================================================================
# 主程序目标
# ============================================================================
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/log_categories.h
    src/log_categories.cpp
    src/log_helper.h
    src/log_helper.cpp
    src/log_formatter.h
    src/log_formatter.cpp
    src/network_module.h
    src/network_module.cpp
    src/database_module.h
    src/database_module.cpp
    src/fileio_module.h
    src/fileio_module.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ============================================================================
# 测试目标
# ============================================================================
enable_testing()

add_executable(tst_log_helper
    tests/tst_log_helper.cpp
    src/log_categories.h
    src/log_categories.cpp
    src/log_helper.h
    src/log_helper.cpp
)

target_link_libraries(tst_log_helper PRIVATE Qt6::Core Qt6::Test)
target_include_directories(tst_log_helper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_test(NAME LogHelperTest COMMAND tst_log_helper)

add_executable(tst_log_formatter
    tests/tst_log_formatter.cpp
    src/log_categories.h
    src/log_categories.cpp
    src/log_formatter.h
    src/log_formatter.cpp
)

target_link_libraries(tst_log_formatter PRIVATE Qt6::Core Qt6::Test)
target_include_directories(tst_log_formatter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_test(NAME LogFormatterTest COMMAND tst_log_formatter)
