add_library(ss_config STATIC config.cpp)
target_include_directories(ss_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_apply_compiler_warnings(ss_config)
add_library(silent_storm::config ALIAS ss_config)

# Unit test
if(BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)
    add_executable(ss_config_test ${CMAKE_SOURCE_DIR}/tests/config/test_config.cpp)
    target_link_libraries(ss_config_test PRIVATE ss_config)
    target_apply_compiler_warnings(ss_config_test)
endif()
