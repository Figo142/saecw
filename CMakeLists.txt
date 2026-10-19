cmake_minimum_required(VERSION 3.16)
project(velox_imgui CXX)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.9)
FetchContent_Declare(glfw  GIT_REPOSITORY https://github.com/glfw/glfw.git     GIT_TAG 3.4)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui glfw)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

add_library(vx STATIC vx/vx.cpp)
target_include_directories(vx PUBLIC vx)
target_link_libraries(vx PUBLIC imgui)

add_executable(velox_demo demo/main.cpp)
target_link_libraries(velox_demo PRIVATE vx)
if(UNIX AND NOT APPLE)
    target_link_libraries(velox_demo PRIVATE GL)
endif()
