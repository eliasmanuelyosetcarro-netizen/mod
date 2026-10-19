cmake_minimum_required(VERSION 3.21)
project(Mimodasistencia CXX)

find_package(Geode REQUIRED)

add_geode_mod(Mimodasistencia)

target_sources(Mimodasistencia PRIVATE
    main.cpp
)
