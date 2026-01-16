cmake_minimum_required(VERSION 3.31)

project(
    cflase
    VERSION 1.0.0
    LANGUAGES CXX
)

set(BUILD_EXAMPLES OFF)
set(ENABLE_PSQL ON)
include(cflib/cmake/ProjectConfig.cmake)

cf_app(cflase
    cflib_dao cflib_db cflib_net
    OTHER_FILES
        .gitignore
    ENABLE_SER
)
