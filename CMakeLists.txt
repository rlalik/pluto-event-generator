cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(PlutoGen
    VERSION 1.0.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

find_package(ROOT 6 REQUIRED COMPONENTS Core)
find_package(Pluto 6.1.9 REQUIRED)

configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/config.h)

add_executable(PlutoGen PlutoGen.cc)

target_include_directories(PlutoGen PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(PlutoGen ROOT::Core ROOT::Hist ROOT::Physics ROOT::Tree Pluto::Pluto)

install(TARGETS PlutoGen
        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${CMAKE_SOURCE_DIR}/ChannelsDatabase.txt
        DESTINATION
        ${CMAKE_INSTALL_DATADIR}/PlutoGen/)
