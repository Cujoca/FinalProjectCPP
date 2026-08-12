// gui_main.cpp — entry point for the Qt Widgets front end.
//
// Author: Bao Vo
//
// The graphical twin of main.cpp. Both entry points sit on top of the exact same
// controller and model:
//
//   view/MainWindow (Qt)   or   view/ConsoleView (terminal)
//                    \          /
//              controller/AppController -> controller/RepositoryManager
//                          |
//     model/Repository, DataManager, DiffEngine, AnalyticsEngine,
//     TrackedFile, Commit, StandardCommit, Validator
//
// Nothing below the view layer knows which front end is running.

#include <QApplication>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QApplication::setApplicationName("MiniVCS");
    QApplication::setOrganizationName("CST8219 Group Project");

    MainWindow window;
    window.show();

    return QApplication::exec();
}
