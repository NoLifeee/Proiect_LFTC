cmake_minimum_required(VERSION 3.30)
project(LAB_LFTC C)

set(CMAKE_C_STANDARD 11)

add_executable(LAB_LFTC main.c
        lexer.c
        utils.c
        parser.c
        ad.c
        ad.h
        at.h
        at.c
        gen.h
        gen.c)
