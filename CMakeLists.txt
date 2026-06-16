cmake_minimum_required(VERSION 4.2)
project(FinalProjectCPP)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(model)

add_executable(FinalProjectCPP
        model/Commit.cpp
        model/Commit.h
        model/DiffEngine.cpp
        model/DiffEngine.h
        model/StandardCommit.cpp
        model/StandardCommit.h
        model/TrackedFile.cpp
        model/TrackedFile.h
        model/Validator.cpp
        model/Validator.h)
