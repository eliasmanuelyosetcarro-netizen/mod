cmake_minimum_required(VERSION 3.21)
project(mi-mod)

find_package(Geode REQUIRED)

add_geode_mod(mi-mod
    src/main.cpp
)
