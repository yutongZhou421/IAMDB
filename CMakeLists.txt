cmake_minimum_required(VERSION 3.20)
project(IAMDB C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# libpg_query (built directly from the Ruby gem's C extension sources)
# ---------------------------------------------------------------------------
set(PG_QUERY_EXT ${CMAKE_SOURCE_DIR}/third_party/pg_query/ext/pg_query)

file(GLOB PG_QUERY_SOURCES
    ${PG_QUERY_EXT}/*.c
    ${PG_QUERY_EXT}/*.cc
)
# Strip Ruby binding and C++ protobuf wrapper (requires full Google protobuf)
list(FILTER PG_QUERY_SOURCES EXCLUDE REGEX "pg_query_ruby\\.c$")
list(FILTER PG_QUERY_SOURCES EXCLUDE REGEX "pg_query_outfuncs_protobuf_cpp\\.cc$")

add_library(pg_query STATIC ${PG_QUERY_SOURCES})
target_include_directories(pg_query PUBLIC
    ${PG_QUERY_EXT}
    ${PG_QUERY_EXT}/include
    ${PG_QUERY_EXT}/include/postgres
)
set_target_properties(pg_query PROPERTIES LINKER_LANGUAGE C)
# Silence warnings from third-party code
target_compile_options(pg_query PRIVATE -w)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(iamdb
    src/main.cpp
    src/planner/expr.cpp
    src/planner/nodes.cpp
    src/planner/builder.cpp
    src/planner/optimizer.cpp
)
target_include_directories(iamdb PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(iamdb PRIVATE pg_query)
