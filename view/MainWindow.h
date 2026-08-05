#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <functional>

#include "RepositoryManager.h"

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;

/* MainWindow — the Qt Widgets front end.
 *
 * Author: Bao Vo
 *
 * The graphical counterpart to ConsoleView, and it follows exactly the same
 * rule: this class owns every widget and every message box, and holds no
 * business logic whatsoever. Each slot is the same three steps the console
 * handlers use — read the widgets, call ONE RepositoryManager method, then show
 * the manager's own explanation in the status bar.
 *
 * Because both front ends talk to the identical controller, the model and
 * controller layers needed no changes at all to gain a GUI.
 *
 * Layout: a QMainWindow wrapping a five-tab QTabWidget
 *   Repository - create / save / load a repository
 *   Files      - track, create, stage and refresh files (Add / Stage live here)
 *   Commits    - commit staged work, browse and search history, restore a file
 *   Diff       - compare a tracked file against any commit
 *   Analytics  - repository statistics
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    // Asks about unsaved work before the window closes (Feature 5).
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Repository tab
    void onInitRepository();
    void onSaveRepository();
    void onLoadRepository();

    // Files tab
    void onAddFile();
    void onBrowseForFile();
    void onCreateFile();
    void onStageFile();
    void onStageAll();
    void onRefreshFile();
    void onShowContent();

    // Commits tab
    void onCommit();
    void onSearchCommits();
    void onRestoreFile();

    // Diff tab
    void onComputeDiff();

    // Live input validation (Feature 1) — re-checks a field as it is typed.
    void onValidateFields();

private:
    // the one and only path to the model
    RepositoryManager manager;

    QTabWidget* tabs = nullptr;

    // Repository tab
    QLineEdit* repoNameEdit = nullptr;
    QLineEdit* repoPathEdit = nullptr;
    QLineEdit* saveFileEdit = nullptr;
    QLabel*    repoSummaryLabel = nullptr;

    // Files tab
    QTableWidget* filesTable = nullptr;
    QLineEdit*    filePathEdit = nullptr;
    QLineEdit*    newFileNameEdit = nullptr;
    QPlainTextEdit* newFileContentEdit = nullptr;
    QPlainTextEdit* fileContentView = nullptr;

    // Commits tab
    QTableWidget* commitsTable = nullptr;
    QLineEdit*    authorEdit = nullptr;
    QLineEdit*    messageEdit = nullptr;
    QLineEdit*    searchEdit = nullptr;

    // Diff tab
    QComboBox*      diffCommitBox = nullptr;
    QComboBox*      diffFileBox = nullptr;
    QPlainTextEdit* diffView = nullptr;

    // Analytics tab
    QLabel* statCommitsLabel = nullptr;
    QLabel* statFilesLabel = nullptr;
    QLabel* statStagedLabel = nullptr;
    QLabel* statMostLabel = nullptr;

    // Warning banner shown while files are staged but not committed (Feature 3).
    QLabel* uncommittedLabel = nullptr;

    // buttons that only make sense once a repository exists
    QList<QWidget*> repoDependentWidgets;

    // True once the repository has changed in a way that has not been saved.
    // Drives the prompt in closeEvent (Feature 5).
    bool unsavedChanges = false;

    // Data file the repository was last saved to / loaded from, remembered
    // between runs so the previous session can be restored on startup.
    QString lastDataFile;

    // ----- construction helpers -----
    QWidget* buildRepositoryTab();
    QWidget* buildFilesTab();
    QWidget* buildCommitsTab();
    QWidget* buildDiffTab();
    QWidget* buildAnalyticsTab();

    // ----- refresh helpers -----
    void refreshAll();
    void refreshFilesTable();
    void refreshCommitsTable();
    void refreshDiffChoices();
    void refreshAnalytics();
    void refreshRepoSummary();
    void setRepoDependentEnabled(bool enabled);

    // ----- small shared helpers -----
    // Shows the manager's message: status bar on success, a dialog on failure.
    void report(bool ok);

    /* Runs a repository operation that touches the file system, turning any
     * escaping std::exception into a QMessageBox::critical instead of letting it
     * terminate the program. Every slot that reads or writes a file goes through
     * here, so a disk error, a permissions problem or a bad conversion is always
     * reported to the user rather than crashing the GUI.
     */
    bool guarded(const QString& action, const std::function<bool()>& operation);

    // Marks the repository as having unsaved work, or clears that state.
    void setUnsavedChanges(bool changed);

    // Offers to save before discarding unsaved work. Returns false if the user
    // chose to cancel whatever prompted the question.
    bool confirmDiscardChanges();

    // Restores the repository saved by the previous session, if there is one.
    void loadPreviousSession();

    // Paints a line edit red (or clears it) and explains why through a tooltip.
    static void markField(QLineEdit* field, const std::string& problem);
    // Path of the row selected in the files table, or an empty string.
    QString selectedFilePath() const;
    // Commit id of the row selected in the commits table, or an empty string.
    QString selectedCommitId() const;
};

#endif // MAINWINDOW_H
