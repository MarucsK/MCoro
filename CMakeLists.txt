#set(CO_ASYNC_DEBUG ON)
#set(CO_ASYNC_ALLOC ON)
#set(CO_ASYNC_EXCEPT ON)
#set(CO_ASYNC_WARN ON)
#set(CO_ASYNC_FIND_LIBURING ON)

cmake_minimum_required(VERSION 3.16)

# if (NOT CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE Release)
# endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT DEFINED CO_ASYNC_DEBUG)
    set(CO_ASYNC_DEBUG ON)
endif()

project(co_async LANGUAGES CXX)

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CXXFLAGS)

    if (CO_ASYNC_WARN)
        list(APPEND CXXFLAGS -Wall -Wextra -Wno-interference-size -Werror=return-type
        -Werror=unused-result -Werror=uninitialized -Werror=maybe-uninitialized
        -Wno-unused-parameter -Wunused-but-set-variable -Wunused-but-set-parameter -Wunused-function
        -Wunused-const-variable -Werror=use-after-free=3 -Werror=invalid-memory-model -Wunused-value
        -Wexceptions -Werror=missing-declarations -Werror=missing-field-initializers
        -Wparentheses -Wconversion -Werror=float-conversion -Wsign-conversion -Wsign-compare -Wno-terminate
        -Werror=delete-non-virtual-dtor -Werror=suggest-override -Wsign-promo -Wold-style-cast
        -Wrange-loop-construct -Werror=reorder -Werror=class-memaccess -Werror=narrowing
        -Wdeprecated-enum-enum-conversion -Werror=deprecated-copy-dtor -Werror=deprecated-copy
        -Wzero-as-null-pointer-constant -Wplacement-new=2 -Wsuggest-final-types -Wsuggest-final-methods
        -Werror=conversion-null -Werror=mismatched-new-delete -Werror=address -Wlogical-op -Wlogical-not-parentheses
        -Wattributes -Wmissing-declarations -Werror=multichar -Werror=overflow -Werror=restrict -Werror=vla
        -Wstrict-aliasing=1 -Werror=string-compare -Werror=stringop-overflow=2 -Werror=shift-overflow
        -Wsuggest-attribute=const -Wsuggest-attribute=pure -Wsuggest-attribute=noreturn -Werror=alloca
        -Werror=array-bounds -Werror=bool-compare -Werror=bool-operation -Werror=zero-length-bounds
        -Werror=div-by-zero -Wno-shadow -Werror=format -Werror=pointer-arith -Werror=write-strings
        -Werror=dangling-pointer=2 -Werror=return-local-addr -Wempty-body -Wimplicit-fallthrough
        -Wswitch -Wno-unknown-warning-option)
    elseif(CO_ASYNC_DEBUG)
        list(APPEND CXXFLAGS -Wall -Wextra -Wno-interference-size -Wreturn-type -Wno-unused-parameter )
    endif()

    if (CO_ASYNC_DEBUG)
        list(APPEND CXXFLAGS -Wno-exceptions)
    else()
        if (CMAKE_BUILD_TYPE MATCHES "[Rr][Ee][Ll][Ee][Aa][Ss][Ee]")
            list(APPEND CXXFLAGS -flto)
        endif()
    endif()

    if (CO_ASYNC_WARN)
        list(APPEND CXXFLAGS -Werror)
    endif()
endif()

include(CheckCXXCompilerFlag)
foreach (flag IN LISTS CXXFLAGS)
    set(name ${flag})
    # replace - and = with _ and remove _ prefix and uppercase
    string(REGEX REPLACE "[-=:]" "_" name ${name})
    string(TOUPPER ${name} name)
    string(REGEX REPLACE "^_" "" name ${name})
    # check if the compiler supports the warning
    check_cxx_compiler_flag(${flag} HAS_${name})
    if (HAS_${name})
        add_compile_options(${flag})
    endif()
endforeach()

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS ${CXXFLAGS})
check_cxx_source_compiles("
#include <coroutine>

int main() {
    std::coroutine_handle<> c;
    (void)c;
    return 0;
}" HAS_COROUTINE)
if (NOT HAS_COROUTINE)
    message(FATAL_ERROR "Your compiler doesn't support coroutine")
endif()

check_cxx_source_compiles("
#include <coroutine>

struct awaiter {
    bool await_ready() const { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const {
        return std::noop_coroutine();
    }
    void await_resume() const {}
};

struct symmetric {
    struct promise_type {
        symmetric get_return_object() { return symmetric{}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

symmetric task() {
    co_await awaiter{};
    co_return;
}

int main() {
    task();
    return 0;
}" HAS_SYMMETRIC_COROUTINE)
if (NOT HAS_SYMMETRIC_COROUTINE)
    message(FATAL_ERROR "Your compiler doesn't support symmetric coroutine transfer")
endif()

add_library(co_async OBJECT)
file(GLOB_RECURSE interface_sources co_async/*.hpp)
file(GLOB_RECURSE implementation_sources co_async/*.cpp)
target_include_directories(co_async PUBLIC .)
target_sources(co_async PRIVATE ${implementation_sources})
if (NOT implementation_sources)
    target_sources(co_async PRIVATE scripts/dummy.cpp)
endif()
if (CO_ASYNC_DEBUG)
     target_compile_definitions(co_async PUBLIC CO_ASYNC_DEBUG)
endif()
if (CO_ASYNC_ALLOC)
    target_compile_definitions(co_async PUBLIC CO_ASYNC_ALLOC)
endif()
if (CO_ASYNC_EXCEPT)
    target_compile_definitions(co_async PUBLIC CO_ASYNC_EXCEPT)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    if (CO_ASYNC_FIND_LIBURING)
        include(FindPkgConfig)
        pkg_check_modules(LIBURING liburing)
        if (NOT LIBURING_FOUND)
            find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
            find_library(LIBURING_LIBRARY NAMES liburing.a)
            if (NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
                message(FATAL_ERROR "liburing not found")
            endif()
            set(LIBURING_LIBRARIES ${LIBURING_LIBRARY})
            set(LIBURING_INCLUDE_DIRS ${LIBURING_INCLUDE_DIR})
        endif()
        target_link_libraries(co_async PUBLIC ${LIBURING_LIBRARIES})
        target_include_directories(co_async PUBLIC ${LIBURING_INCLUDE_DIRS})
    else()
        target_sources(co_async PRIVATE 3rd/liburing/liburing.cpp)
        target_include_directories(co_async PUBLIC 3rd/liburing/include)
    endif()
endif()
target_include_directories(co_async PUBLIC 3rd/debug/include)

find_package(Threads REQUIRED)
target_link_libraries(co_async PUBLIC Threads::Threads)

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    file(GLOB example_sources examples/*.cpp)
    foreach(path ${example_sources})
        get_filename_component(name ${path} NAME_WE)
        add_executable(${name} ${path})
        target_link_libraries(${name} PRIVATE co_async)
    endforeach()
endif()
