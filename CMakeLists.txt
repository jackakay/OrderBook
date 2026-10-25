cmake_minimum_required(VERSION 3.14)
project(MarketGui C CXX) # C is needed for glad.c

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Automatically download GLFW from GitHub
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Automatically download Dear ImGui from GitHub
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# Find OpenGL on your system automatically
find_package(OpenGL REQUIRED)

# Create your executable, linking source files and ImGui backend files directly
add_executable(MarketGui
    src/main.cpp
    src/order_book.cpp
    src/order_book.h
    src/matching_engine.cpp
    src/matching_engine.h
    src/ui_interface.cpp
    src/ui_interface.h
    src/glad.c
    src/ImPlot/implot.cpp
    src/ImPlot/implot_items.cpp
    src/ImPlot/implot_internal.h
    src/ImPlot/implot.h

    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Tell the compiler where to find header files
target_include_directories(MarketGui PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Fonts are loaded at runtime from the repo's fonts/ folder
target_compile_definitions(MarketGui PRIVATE FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts")

# Link everything together
target_link_libraries(MarketGui PRIVATE glfw OpenGL::GL)