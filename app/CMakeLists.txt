cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(app C CXX)

add_subdirectory(.. ${PROJECT_BINARY_DIR}/egn)

set(APP_SOURCES 
    "main.cpp"
    "Controller.cpp"
    "Scene.cpp"
)

add_executable(app ${APP_SOURCES})
target_link_libraries(app PRIVATE engine)
target_include_directories(app PRIVATE "src")
