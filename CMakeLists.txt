cmake_minimum_required(VERSION 3.20)
project(FinalProjectCPP CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target)

# ---- Shared sources -------------------------------------------------------
# Listed explicitly rather than globbed: the console front end, the Qt front end
# and the test runner each pick the pieces they need, and a new GUI file must
# never be swept into the console build by accident.

set(MODEL_SOURCES
    model/Commit.cpp
    model/StandardCommit.cpp
    model/TrackedFile.cpp
    model/Validator.cpp
    model/DiffEngine.cpp
    model/Repository.cpp
    model/DataManager.cpp
)

# The controller is the only thing either front end talks to.
set(CONTROLLER_SOURCES
    controller/RepositoryManager.cpp
)

# The console presentation layer (also holds the pure formatting helpers that
# the test suite exercises directly).
set(CONSOLE_VIEW_SOURCES
    view/ConsoleView.cpp
)

# ---- Console application --------------------------------------------------
add_executable(${PROJECT_NAME}
    main.cpp
    ${MODEL_SOURCES}
    ${CONTROLLER_SOURCES}
    ${CONSOLE_VIEW_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    model
    view
    controller
)

# ---- Test target ----------------------------------------------------------
add_executable(TestRunner
    tests/test_main.cpp
    ${MODEL_SOURCES}
    ${CONSOLE_VIEW_SOURCES}
    ${CONTROLLER_SOURCES}
)

set_target_properties(TestRunner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
)

target_include_directories(TestRunner PRIVATE model view controller)

# $<TARGET_FILE:...> resolves to the real binary for whichever generator is in
# use — tests/TestRunner with Makefiles, tests/Debug/TestRunner.exe with the
# multi-config Visual Studio generator.
add_custom_target(RunTests
    COMMAND $<TARGET_FILE:TestRunner> > ${CMAKE_SOURCE_DIR}/tests/test_output.txt 2>&1
    DEPENDS TestRunner
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running tests - output written to tests/test_output.txt"
)

# ---- Qt Widgets application ----------------------------------------------
# Optional: the console app and the tests still build with plain MSVC when Qt is
# not present. Configure with -DCMAKE_PREFIX_PATH=<Qt kit> to enable it, e.g.
#   cmake -S . -B build-qt -G "MinGW Makefiles" \
#         -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
find_package(Qt6 COMPONENTS Widgets QUIET)

if (Qt6_FOUND)
    message(STATUS "Qt6 found - building the MiniVCSGui target")

    add_executable(MiniVCSGui
        view/gui_main.cpp
        view/MainWindow.cpp
        view/MainWindow.h
        ${MODEL_SOURCES}
        ${CONTROLLER_SOURCES}
    )

    # MainWindow uses Q_OBJECT, so it has to run through the meta-object compiler.
    set_target_properties(MiniVCSGui PROPERTIES
        AUTOMOC ON
        WIN32_EXECUTABLE ON   # no console window behind the GUI
    )

    target_include_directories(MiniVCSGui PRIVATE model view controller)
    target_link_libraries(MiniVCSGui PRIVATE Qt6::Widgets)

    # Functional tests that drive the real MainWindow widgets.
    add_executable(GuiTestRunner
        tests/gui_test.cpp
        view/MainWindow.cpp
        view/MainWindow.h
        ${MODEL_SOURCES}
        ${CONTROLLER_SOURCES}
    )

    set_target_properties(GuiTestRunner PROPERTIES AUTOMOC ON)
    target_include_directories(GuiTestRunner PRIVATE model view controller)
    target_link_libraries(GuiTestRunner PRIVATE Qt6::Widgets)

    # Runs headless via the offscreen platform plugin, so it needs no display.
    add_custom_target(RunGuiTests
        COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
                $<TARGET_FILE:GuiTestRunner> > ${CMAKE_SOURCE_DIR}/tests/gui_test_output.txt 2>&1
        DEPENDS GuiTestRunner
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running GUI tests - output written to tests/gui_test_output.txt"
    )
else()
    message(STATUS "Qt6 not found - skipping the MiniVCSGui target "
                   "(set CMAKE_PREFIX_PATH to your Qt kit to enable it)")
endif()
