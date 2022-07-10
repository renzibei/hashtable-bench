cmake_minimum_required(VERSION 3.10)
project(hashtable_bench)

set(CMAKE_CXX_STANDARD 17)
set(ABSL_PROPAGATE_CXX_STD 17)

set(MEMCHECK_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
set(MEMCHECK_FLAGS "")

# for Apple Silicon
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=native")
endif()

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra ${MEMCHECK_FLAGS}")
endif()

OPTION(BENCH_ONLY_STRING "Benchmark only the string key" OFF)
OPTION(BENCH_ONLY_INT "Benchmark only the uint64_t key" OFF)
OPTION(BENCH_LATENCY_WHEN_AVAILABLE "Measure the latency when it is available" OFF)
OPTION(FORCE_BENCH_LATENCY "Force to benchmark the latency" OFF)
OPTION(USE_RAW_HISTOGRAM "Use raw_histogram, otherwise use HdrHistogram_c" OFF)

IF(BENCH_ONLY_STRING)
    ADD_DEFINITIONS(-DBENCH_ONLY_STRING)
ENDIF(BENCH_ONLY_STRING)

IF(BENCH_ONLY_INT)
    ADD_DEFINITIONS(-DBENCH_ONLY_INT)
ENDIF(BENCH_ONLY_INT)

IF(BENCH_LATENCY_WHEN_AVAILABLE)
    ADD_DEFINITIONS(-DBENCH_LATENCY_WHEN_AVAILABLE)
ENDIF(BENCH_LATENCY_WHEN_AVAILABLE)

IF(FORCE_BENCH_LATENCY)
    ADD_DEFINITIONS(-DFORCE_BENCH_LATENCY)
ENDIF(FORCE_BENCH_LATENCY)

file(GLOB INC_HASHES "src/hashes/*")
file(GLOB INC_MAPS "src/maps/*")
file(GLOB INC_UTILS "src/utils/")
file(GLOB INC_SEED_HASHES "src/seed-hashes/*")
file(GLOB INC_SEED_MAPS "src/seed-maps/*")

set(BENCH_SOURCES src/benchmark.cpp)
# can use huge pages in linux
set(ALLOCATOR_DIR src/allocators/std_allocator)
#set(ALLOCATOR_DIR src/allocators/thp_allocator)

find_package(Threads REQUIRED)


#add_subdirectory(thirdparty/fph-table)
add_subdirectory(thirdparty/fph-table-noseed)
add_subdirectory(thirdparty/abseil-cpp)
add_subdirectory(thirdparty/tsl-robin-map)

if(USE_RAW_HISTOGRAM)
    ADD_DEFINITIONS(-DUSE_RAW_HISTOGRAM)
    SET(HDR_HIST_LIB "")
else()
    add_subdirectory(thirdparty/HdrHistogram_c)
    SET(HDR_HIST_LIB hdr_histogram_static)
endif()

set(XXHASH_BUILD_XXHSUM OFF)

add_subdirectory(thirdparty/xxHash/cmake_unofficial)


foreach(MAP_DIR ${INC_SEED_MAPS})
    if (IS_DIRECTORY ${MAP_DIR})
    get_filename_component(MAP_NAME ${MAP_DIR} NAME_WE)
        foreach(HASH_DIR ${INC_SEED_HASHES})
            if (IS_DIRECTORY ${HASH_DIR})
                # executable name: mapname_hashname
                get_filename_component(HASH_NAME ${HASH_DIR} NAME_WE)
                set(EXECUTABLE_NAME "bench_${MAP_NAME}__${HASH_NAME}")

                # collect .cpp files in current hash and map directory
                file(GLOB SRC_MAP_DIR "${MAP_DIR}/*.cpp")
                file(GLOB SRC_HASH_DIR "${HASH_DIR}/*.cpp")

                add_executable(${EXECUTABLE_NAME} ${SRC_APP} ${BENCH_SOURCES} ${SRC_MAP_DIR} ${SRC_HASH_DIR})
                target_link_libraries(${EXECUTABLE_NAME} fph::fph_table ${HDR_HIST_LIB})
                target_include_directories(${EXECUTABLE_NAME} PRIVATE "thirdparty" ${MAP_DIR} ${HASH_DIR} ${ALLOCATOR_DIR} ${INC_UTILS})

                if (EXISTS "${MAP_DIR}/dependencies.cmake")
                    include("${MAP_DIR}/dependencies.cmake")
                endif ()

                if (EXISTS "${HASH_DIR}/dependencies.cmake")
                    include("${HASH_DIR}/dependencies.cmake")
                endif ()
            endif()
        endforeach(HASH_DIR ${INC_SEED_HASHES})
    endif()
endforeach(MAP_DIR ${INC_SEED_MAPS})

foreach(MAP_DIR ${INC_MAPS})
    if (IS_DIRECTORY ${MAP_DIR})
        get_filename_component(MAP_NAME ${MAP_DIR} NAME_WE)
        foreach(HASH_DIR ${INC_HASHES})
            if (IS_DIRECTORY ${HASH_DIR})
                # executable name: mapname_hashname
                get_filename_component(HASH_NAME ${HASH_DIR} NAME_WE)
                set(EXECUTABLE_NAME "bench_${MAP_NAME}__${HASH_NAME}")

                # collect .cpp files in current hash and map directory
                file(GLOB SRC_MAP_DIR "${MAP_DIR}/*.cpp")
                file(GLOB SRC_HASH_DIR "${HASH_DIR}/*.cpp")

                add_executable(${EXECUTABLE_NAME} ${SRC_APP} ${BENCH_SOURCES} ${SRC_MAP_DIR} ${SRC_HASH_DIR})
                target_link_libraries(${EXECUTABLE_NAME} fph::fph_table ${HDR_HIST_LIB})
                target_include_directories(${EXECUTABLE_NAME} PRIVATE "thirdparty" ${MAP_DIR} ${HASH_DIR} ${ALLOCATOR_DIR} ${INC_UTILS})

                if (EXISTS "${MAP_DIR}/dependencies.cmake")
                    include("${MAP_DIR}/dependencies.cmake")
                endif ()

                if (EXISTS "${HASH_DIR}/dependencies.cmake")
                    include("${HASH_DIR}/dependencies.cmake")
                endif ()
            endif()
        endforeach(HASH_DIR ${INC_HASHES})
    endif()
endforeach(MAP_DIR ${INC_MAPS})