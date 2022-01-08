cmake_minimum_required(VERSION 3.10)
project(hashtable_bench)

set(CMAKE_CXX_STANDARD 17)
set(ABSL_PROPAGATE_CXX_STD 17)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-format")

file(GLOB INC_HASHES "src/hashes/*")
file(GLOB INC_MAPS "src/maps/*")
file(GLOB INC_SEED_HASHES "src/seed-hashes/*")
file(GLOB INC_SEED_MAPS "src/seed-maps/*")

set(BENCH_SOURCES src/benchmark.cpp)
set(ALLOCATOR_DIR src/allocators/std_allocator)

find_package(Threads REQUIRED)


add_subdirectory(thirdparty/fph-table)
add_subdirectory(thirdparty/abseil-cpp)

foreach(MAP_DIR ${INC_SEED_MAPS})
    get_filename_component(MAP_NAME ${MAP_DIR} NAME_WE)
    foreach(HASH_DIR ${INC_SEED_HASHES})
        # executable name: mapname_hashname
        get_filename_component(HASH_NAME ${HASH_DIR} NAME_WE)
        set(EXECUTABLE_NAME "bench_${MAP_NAME}__${HASH_NAME}")

        # collect .cpp files in current hash and map directory
        file(GLOB SRC_MAP_DIR "${MAP_DIR}/*.cpp")
        file(GLOB SRC_HASH_DIR "${HASH_DIR}/*.cpp")

        add_executable(${EXECUTABLE_NAME} ${SRC_APP} ${BENCH_SOURCES} ${SRC_MAP_DIR} ${SRC_HASH_DIR})
        target_link_libraries(${EXECUTABLE_NAME} fph::fph_table)
        target_include_directories(${EXECUTABLE_NAME} PRIVATE "thirdparty" ${MAP_DIR} ${HASH_DIR} ${ALLOCATOR_DIR})

        if (EXISTS "${MAP_DIR}/dependencies.cmake")
            include("${MAP_DIR}/dependencies.cmake")
        endif ()

        if (EXISTS "${HASH_DIR}/dependencies.cmake")
            include("${HASH_DIR}/dependencies.cmake")
        endif ()

    endforeach(HASH_DIR ${INC_SEED_HASHES})
endforeach(MAP_DIR ${INC_SEED_MAPS})

foreach(MAP_DIR ${INC_MAPS})
    get_filename_component(MAP_NAME ${MAP_DIR} NAME_WE)
    foreach(HASH_DIR ${INC_HASHES})
        # executable name: mapname_hashname
        get_filename_component(HASH_NAME ${HASH_DIR} NAME_WE)
        set(EXECUTABLE_NAME "bench_${MAP_NAME}__${HASH_NAME}")

        # collect .cpp files in current hash and map directory
        file(GLOB SRC_MAP_DIR "${MAP_DIR}/*.cpp")
        file(GLOB SRC_HASH_DIR "${HASH_DIR}/*.cpp")

        add_executable(${EXECUTABLE_NAME} ${SRC_APP} ${BENCH_SOURCES} ${SRC_MAP_DIR} ${SRC_HASH_DIR})
        target_link_libraries(${EXECUTABLE_NAME} fph::fph_table)
        target_include_directories(${EXECUTABLE_NAME} PRIVATE "thirdparty" ${MAP_DIR} ${HASH_DIR} ${ALLOCATOR_DIR})

        if (EXISTS "${MAP_DIR}/dependencies.cmake")
            include("${MAP_DIR}/dependencies.cmake")
        endif ()

        if (EXISTS "${HASH_DIR}/dependencies.cmake")
            include("${HASH_DIR}/dependencies.cmake")
        endif ()

    endforeach(HASH_DIR ${INC_HASHES})
endforeach(MAP_DIR ${INC_MAPS})