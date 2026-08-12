// gui_test.cpp — functional tests for the Qt Widgets front end.
//
// Author: Bao Vo
//
// These tests drive the REAL MainWindow: they look widgets up by object name,
// type into them, click the actual buttons, and then assert on what the tables,
// combo boxes and labels display. That exercises the signal/slot wiring end to
// end — a window that compiles but whose buttons are connected to the wrong slot
// would fail here.
//
// Run headless with:  QT_QPA_PLATFORM=offscreen ./target/GuiTestRunner
//
// Failure paths open a modal QMessageBox, which would block a non-interactive
// run forever, so a timer below closes any dialog that appears and counts it.
// That lets the suite click refusals as well as successes; the wording of each
// message is covered by the AppController tests in test_main.cpp.

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTimer>
#include <QTableWidget>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "MainWindow.h"

using namespace std;

// ---- minimal test harness (same style as test_main.cpp) ----
static int g_passed = 0, g_failed = 0;

// counts modal dialogs the timer in main() had to dismiss
static int g_dialogsShown = 0;

#define CHECK(expr) \
    do { \
        if (expr) { cout << "[PASS] " << #expr << "\n"; ++g_passed; } \
        else      { cout << "[FAIL] " << #expr << "\n"; ++g_failed; } \
    } while (0)

#define SECTION(name) cout << "\n=== " << (name) << " ===\n"

static const QString TMP_DIR = "tests/tmp_gui_files";

// Typed widget lookup by object name — fails loudly rather than crashing later.
template <typename T>
static T* find(MainWindow& window, const char* name) {
    T* widget = window.findChild<T*>(name);
    if (widget == nullptr) {
        cout << "[FAIL] could not find widget '" << name << "'\n";
        ++g_failed;
    }
    return widget;
}

static void click(MainWindow& window, const char* name) {
    if (auto* button = find<QPushButton>(window, name)) button->click();
}

static void type(MainWindow& window, const char* name, const QString& text) {
    if (auto* edit = find<QLineEdit>(window, name)) edit->setText(text);
}

// Optional screenshot capture, handy for the project write-up. Enable with:
//   MINIVCS_GUI_SHOTS=docs/screenshots  QT_QPA_PLATFORM=offscreen ./target/GuiTestRunner
// Off by default so a normal test run leaves no files behind.
static void snap(MainWindow& window, int tabIndex, const QString& name) {
    const QByteArray dir = qgetenv("MINIVCS_GUI_SHOTS");
    if (dir.isEmpty()) return;

    filesystem::create_directories(dir.toStdString());
    if (auto* tabs = window.findChild<QTabWidget*>()) tabs->setCurrentIndex(tabIndex);
    window.resize(1000, 680);
    window.grab().save(QString::fromUtf8(dir) + "/" + name + ".png");
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // QSettings needs these to resolve a storage location; the startup-restore
    // feature is driven through it, so the names must match the real app.
    QApplication::setApplicationName("MiniVCS");
    QApplication::setOrganizationName("CST8219 Group Project");

    /* Modal dialogs run their own nested event loop, so a QTimer still fires
     * while one is up. Closing it lets exec() return and the test carry on -
     * without this a single refusal dialog stalls the whole run.
     */
    QTimer dialogCloser;
    QObject::connect(&dialogCloser, &QTimer::timeout, [] {
        if (QWidget* modal = QApplication::activeModalWidget()) {
            g_dialogsShown++;
            modal->close();
        }
    });
    dialogCloser.start(20);

    cout << "========================================\n";
    cout << "  MiniVCS GUI Test Suite\n";
    cout << "========================================\n";

    // start from a clean slate so a previous run cannot restore a session into
    // the middle of these tests
    QSettings().clear();

    filesystem::remove_all(TMP_DIR.toStdString());
    filesystem::create_directories(TMP_DIR.toStdString());

    MainWindow window;

    auto* filesTable   = find<QTableWidget>(window, "filesTable");
    auto* commitsTable = find<QTableWidget>(window, "commitsTable");

    SECTION("GUI: repository-dependent actions start disabled");
    {
        auto* stageAll = find<QPushButton>(window, "stageAllButton");
        auto* commit   = find<QPushButton>(window, "commitButton");
        auto* load     = find<QPushButton>(window, "loadRepoButton");
        CHECK(stageAll != nullptr && !stageAll->isEnabled());
        CHECK(commit   != nullptr && !commit->isEnabled());
        // loading is the one thing that works without an open repository
        CHECK(load     != nullptr && load->isEnabled());
    }

    SECTION("GUI: creating a repository enables the rest of the window");
    {
        type(window, "repoNameEdit", "gui-repo");
        type(window, "repoPathEdit", TMP_DIR);
        click(window, "createRepoButton");

        auto* stageAll = find<QPushButton>(window, "stageAllButton");
        CHECK(stageAll != nullptr && stageAll->isEnabled());

        auto* summary = window.findChild<QLabel*>();
        CHECK(summary != nullptr);
    }

    SECTION("GUI: creating a file adds a row to the files table");
    {
        type(window, "newFileNameEdit", "gui.txt");
        if (auto* content = find<QPlainTextEdit>(window, "newFileContentEdit")) {
            content->setPlainText("first line\n");
        }
        click(window, "createFileButton");

        CHECK(filesTable != nullptr && filesTable->rowCount() == 1);
        CHECK(filesTable->item(0, 0)->text() == "gui.txt");
        CHECK(filesTable->item(0, 1)->text() == "Modified");
        // the input boxes are cleared after a successful create
        CHECK(find<QLineEdit>(window, "newFileNameEdit")->text().isEmpty());
    }

    SECTION("GUI: 'Stage all modified' updates the status column");
    {
        click(window, "stageAllButton");
        CHECK(filesTable->item(0, 1)->text() == "Staged");
        CHECK(find<QLabel>(window, "statStagedLabel")->text() == "1");
    }

    SECTION("GUI: uncommitted-changes warning appears while work is staged (Feature 3)");
    {
        auto* warning = find<QLabel>(window, "uncommittedLabel");
        CHECK(warning != nullptr && warning->isVisibleTo(&window));
        CHECK(warning->text().contains("staged but not committed"));
    }

    SECTION("GUI: unsaved work is marked in the title bar (Feature 5)");
    {
        CHECK(window.windowTitle().contains("*"));
        CHECK(window.windowTitle().contains("gui-repo"));
    }

    SECTION("GUI: invalid input is highlighted as it is typed (Feature 1)");
    {
        auto* name = find<QLineEdit>(window, "repoNameEdit");

        name->setText("ab");                       // shorter than 3 characters
        CHECK(!name->styleSheet().isEmpty());      // painted red
        CHECK(name->toolTip().contains("too short"));

        name->setText("12345");                    // no alphabetic character
        CHECK(name->toolTip().contains("letter"));

        name->setText("gui-repo");                 // valid again
        CHECK(name->styleSheet().isEmpty());
        CHECK(name->toolTip().isEmpty());

        auto* author = find<QLineEdit>(window, "authorEdit");
        author->setText("12345");
        CHECK(!author->styleSheet().isEmpty());
        author->setText("Bao Vo");
        CHECK(author->styleSheet().isEmpty());
    }

    SECTION("GUI: committing fills the commits table and clears the message");
    {
        type(window, "authorEdit", "Bao Vo");
        type(window, "messageEdit", "first gui commit");
        click(window, "commitButton");

        CHECK(commitsTable != nullptr && commitsTable->rowCount() == 1);
        CHECK(commitsTable->item(0, 2)->text() == "Bao Vo");
        CHECK(commitsTable->item(0, 3)->text() == "first gui commit");
        // committing moves the file on and refreshes the files table
        CHECK(filesTable->item(0, 1)->text() == "Committed");
        CHECK(find<QLineEdit>(window, "messageEdit")->text().isEmpty());
        // the author is deliberately kept for the next commit
        CHECK(find<QLineEdit>(window, "authorEdit")->text() == "Bao Vo");
    }

    SECTION("GUI: analytics tab reflects the commit");
    {
        CHECK(find<QLabel>(window, "statCommitsLabel")->text() == "1");
        CHECK(find<QLabel>(window, "statFilesLabel")->text()   == "1");
        CHECK(find<QLabel>(window, "statStagedLabel")->text()  == "0");
        CHECK(find<QLabel>(window, "statMostLabel")->text().contains("gui.txt"));
    }

    SECTION("GUI: 'Show content' displays the tracked file");
    {
        filesTable->selectRow(0);
        click(window, "showContentButton");
        CHECK(find<QPlainTextEdit>(window, "fileContentView")->toPlainText().contains("first line"));
    }

    SECTION("GUI: the diff tab is populated and computes a real diff");
    {
        // edit the file behind the app's back, then re-read it
        {
            ofstream out((TMP_DIR + "/gui.txt").toStdString());
            out << "first line\nsecond line\n";
        }
        filesTable->selectRow(0);
        click(window, "refreshFileButton");
        CHECK(filesTable->item(0, 1)->text() == "Modified");

        auto* commitBox = find<QComboBox>(window, "diffCommitBox");
        auto* fileBox   = find<QComboBox>(window, "diffFileBox");
        CHECK(commitBox != nullptr && commitBox->count() == 1);
        CHECK(fileBox   != nullptr && fileBox->count()   == 1);

        click(window, "diffButton");
        const QString diff = find<QPlainTextEdit>(window, "diffView")->toPlainText();

        /* DiffEngine prints both versions in full under "Old Content:" /
         * "New Content:" headings rather than marking individual lines, so the
         * panel is checked for that shape and for the added text appearing.
         */
        CHECK(diff.contains("Old Content:"));
        CHECK(diff.contains("New Content:"));
        CHECK(diff.contains("second line"));
    }

    SECTION("GUI: search narrows the commits table and keeps every column");
    {
        // a second commit so there is something to filter out
        filesTable->selectRow(0);
        click(window, "stageButton");
        type(window, "messageEdit", "second gui commit");
        click(window, "commitButton");
        CHECK(commitsTable->rowCount() == 2);

        type(window, "searchEdit", "second");
        click(window, "searchButton");
        CHECK(commitsTable->rowCount() == 1);
        CHECK(commitsTable->item(0, 3)->text() == "second gui commit");
        // the commit id column must still be filled, otherwise a row picked out
        // of a search result could not be used to restore a file
        CHECK(!commitsTable->item(0, 1)->text().isEmpty());

        type(window, "searchEdit", "");
        click(window, "searchButton");
        CHECK(commitsTable->rowCount() == 2);

        // capture each populated tab if screenshots were requested
        snap(window, 0, "1-repository");
        snap(window, 1, "2-files");
        snap(window, 2, "3-commits");
        snap(window, 3, "4-diff");
        snap(window, 4, "5-analytics");
    }

    SECTION("GUI: the uncommitted warning clears once everything is committed");
    {
        auto* warning = find<QLabel>(window, "uncommittedLabel");
        CHECK(warning != nullptr && !warning->isVisibleTo(&window));
    }

    SECTION("GUI: saving writes the file and clears the unsaved marker (Feature 4/5)");
    {
        type(window, "saveFileEdit", TMP_DIR + "/gui.dat");
        CHECK(window.windowTitle().contains("*"));      // unsaved before
        click(window, "saveRepoButton");
        CHECK(!window.windowTitle().contains("*"));     // and clean after
        CHECK(filesystem::exists((TMP_DIR + "/gui.dat").toStdString()));
    }

    SECTION("GUI: saving is refused when the data file name is blank (Feature 4)");
    {
        auto* saveEdit = find<QLineEdit>(window, "saveFileEdit");
        const QString good = saveEdit->text();

        saveEdit->setText("   ");
        click(window, "saveRepoButton");
        CHECK(!saveEdit->styleSheet().isEmpty());       // flagged instead of saved

        saveEdit->setText(good);
    }

    SECTION("GUI: a new window reopens the previous session on startup");
    {
        // the save above recorded the data file, so a fresh window restores it
        MainWindow reopened;
        auto* files   = find<QTableWidget>(reopened, "filesTable");
        auto* commits = find<QTableWidget>(reopened, "commitsTable");

        CHECK(files   != nullptr && files->rowCount()   == 1);
        CHECK(commits != nullptr && commits->rowCount() == 2);
        CHECK(reopened.findChild<QLineEdit*>("repoNameEdit")->text() == "gui-repo");
        CHECK(reopened.findChild<QLabel*>("statCommitsLabel")->text() == "2");
        // a freshly restored session has nothing unsaved
        CHECK(!reopened.windowTitle().contains("*"));
    }

    SECTION("GUI: loading explicitly also restores the repository");
    {
        QSettings().remove("lastDataFile");   // no automatic restore this time

        MainWindow manual;
        auto* files = find<QTableWidget>(manual, "filesTable");
        CHECK(files != nullptr && files->rowCount() == 0);

        QLineEdit* saveEdit = manual.findChild<QLineEdit*>("saveFileEdit");
        if (saveEdit) saveEdit->setText(TMP_DIR + "/gui.dat");
        click(manual, "loadRepoButton");

        CHECK(files->rowCount() == 1);
        CHECK(manual.findChild<QTableWidget*>("commitsTable")->rowCount() == 2);
        CHECK(manual.findChild<QLineEdit*>("repoNameEdit")->text() == "gui-repo");
    }

    SECTION("GUI: a corrupt saved session is survived on startup (exception safety)");
    {
        // A truncated data file: names a file count it never provides.
        {
            ofstream out((TMP_DIR + "/broken.dat").toStdString());
            out << "broken-repo\nsome/path\n5\n";
        }
        QSettings().setValue("lastDataFile", TMP_DIR + "/broken.dat");

        // The startup restore runs through guarded(); a failure there reports to
        // the status bar rather than opening a modal box, so it is safe to drive
        // here — and the window must still come up usable.
        MainWindow broken;
        CHECK(broken.findChild<QTableWidget*>("filesTable")->rowCount() == 0);
        CHECK(broken.findChild<QTableWidget*>("commitsTable")->rowCount() == 0);
        // an unreadable session must not leave the app pretending it has one
        CHECK(!broken.findChild<QPushButton*>("commitButton")->isEnabled());
    }

    SECTION("GUI: a missing saved session is ignored quietly");
    {
        QSettings().setValue("lastDataFile", TMP_DIR + "/does-not-exist.dat");
        MainWindow missing;
        CHECK(missing.findChild<QTableWidget*>("filesTable")->rowCount() == 0);
        CHECK(!missing.findChild<QPushButton*>("stageAllButton")->isEnabled());
    }

    SECTION("GUI: refusals really do open a dialog, they are not swallowed");
    {
        /* The run above clicked at least one refusal (saving with a blank data
         * file name), so the dismisser must have closed a dialog. If this ever
         * reads zero, the dialog helpers have stopped showing anything and every
         * "invalid input is reported" test above would be passing vacuously.
         */
        CHECK(g_dialogsShown > 0);
        cout << "       (" << g_dialogsShown << " dialog(s) shown and dismissed)\n";
    }

    QSettings().clear();

    filesystem::remove_all(TMP_DIR.toStdString());

    cout << "\n----------------------------------------\n";
    cout << "Results: " << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
