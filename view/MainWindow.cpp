#include "MainWindow.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <set>

namespace {

// std::string <-> QString sit on the boundary between the model and Qt, so the
// conversion is kept to these two helpers rather than sprinkled everywhere.
QString toQ(const std::string& text) {
    return QString::fromStdString(text);
}

std::string toStd(const QString& text) {
    return text.toStdString();
}

// A read-only table cell — the tables display model state and are never edited
// in place.
QTableWidgetItem* cell(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

// Monospaced font so diffs and file contents line up column for column.
QFont monospaceFont() {
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    return font;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setWindowTitle("MiniVCS - C++ Version Control System");
    resize(1000, 680);

    tabs = new QTabWidget(this);
    tabs->addTab(buildRepositoryTab(), "Repository");
    tabs->addTab(buildFilesTab(),      "Files");
    tabs->addTab(buildCommitsTab(),    "Commits");
    tabs->addTab(buildDiffTab(),       "Diff");
    tabs->addTab(buildAnalyticsTab(),  "Analytics");
    setCentralWidget(tabs);

    statusBar()->showMessage("Create or load a repository to begin.");

    // nothing but the Repository tab is usable until a repository exists
    setRepoDependentEnabled(false);
    refreshAll();
    setUnsavedChanges(false);

    // live validation as the user types (Feature 1)
    for (QLineEdit* field : {repoNameEdit, repoPathEdit, authorEdit, messageEdit}) {
        connect(field, &QLineEdit::textChanged, this, &MainWindow::onValidateFields);
    }

    // bring back whatever the previous run was working on
    loadPreviousSession();
}

// ---------------------------------------------------------------------------
// Tab construction
// ---------------------------------------------------------------------------

QWidget* MainWindow::buildRepositoryTab() {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // --- create ---
    auto* createBox    = new QGroupBox("Create a repository", page);
    auto* createForm   = new QFormLayout(createBox);
    repoNameEdit       = new QLineEdit(createBox);
    repoPathEdit       = new QLineEdit(".", createBox);
    repoNameEdit->setPlaceholderText("3-50 characters, at least one letter");

    auto* browsePathButton = new QPushButton("Browse...", createBox);
    auto* pathRow          = new QHBoxLayout;
    pathRow->addWidget(repoPathEdit);
    pathRow->addWidget(browsePathButton);

    auto* createButton = new QPushButton("Create repository", createBox);
    createForm->addRow("Name:", repoNameEdit);
    createForm->addRow("Path:", pathRow);
    createForm->addRow("", createButton);

    connect(browsePathButton, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Choose a repository folder");
        if (!dir.isEmpty()) repoPathEdit->setText(dir);
    });
    connect(createButton,  &QPushButton::clicked, this, &MainWindow::onInitRepository);
    connect(repoNameEdit,  &QLineEdit::returnPressed, this, &MainWindow::onInitRepository);

    // --- persistence ---
    auto* saveBox  = new QGroupBox("Save / load", page);
    auto* saveForm = new QFormLayout(saveBox);
    saveFileEdit   = new QLineEdit("minivcs.dat", saveBox);

    auto* saveButton = new QPushButton("Save repository", saveBox);
    auto* loadButton = new QPushButton("Load repository", saveBox);
    auto* saveRow    = new QHBoxLayout;
    saveRow->addWidget(saveButton);
    saveRow->addWidget(loadButton);
    saveRow->addStretch();

    saveForm->addRow("Data file:", saveFileEdit);
    saveForm->addRow("", saveRow);

    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveRepository);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadRepository);

    // loading is the one action that works without an open repository
    repoDependentWidgets.append(saveButton);

    // --- summary ---
    repoSummaryLabel = new QLabel(page);
    repoSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(createBox);
    layout->addWidget(saveBox);
    layout->addWidget(repoSummaryLabel);
    layout->addStretch();

    // Object names let tests/gui_test.cpp find and drive these widgets by name.
    repoNameEdit->setObjectName("repoNameEdit");
    repoPathEdit->setObjectName("repoPathEdit");
    saveFileEdit->setObjectName("saveFileEdit");
    createButton->setObjectName("createRepoButton");
    saveButton->setObjectName("saveRepoButton");
    loadButton->setObjectName("loadRepoButton");

    return page;
}

QWidget* MainWindow::buildFilesTab() {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // --- the tracked files table ---
    filesTable = new QTableWidget(0, 3, page);
    filesTable->setHorizontalHeaderLabels({"Path", "Status", "Size (bytes)"});
    filesTable->horizontalHeader()->setStretchLastSection(false);
    filesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    filesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    filesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    filesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    filesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // --- track an existing file ---
    auto* trackBox = new QGroupBox("Track an existing file", page);
    auto* trackRow = new QHBoxLayout(trackBox);
    filePathEdit   = new QLineEdit(trackBox);
    filePathEdit->setPlaceholderText("path to a file that already exists on disk");

    auto* browseButton = new QPushButton("Browse...", trackBox);
    auto* addButton    = new QPushButton("Add", trackBox);
    trackRow->addWidget(filePathEdit);
    trackRow->addWidget(browseButton);
    trackRow->addWidget(addButton);

    connect(browseButton,  &QPushButton::clicked, this, &MainWindow::onBrowseForFile);
    connect(addButton,     &QPushButton::clicked, this, &MainWindow::onAddFile);
    connect(filePathEdit,  &QLineEdit::returnPressed, this, &MainWindow::onAddFile);

    // --- create a new file ---
    auto* createBox    = new QGroupBox("Create and track a new file", page);
    auto* createLayout = new QVBoxLayout(createBox);
    newFileNameEdit    = new QLineEdit(createBox);
    newFileNameEdit->setPlaceholderText("path for the new file");
    newFileContentEdit = new QPlainTextEdit(createBox);
    newFileContentEdit->setPlaceholderText("file content");
    newFileContentEdit->setFont(monospaceFont());
    newFileContentEdit->setMaximumHeight(90);

    auto* createButton = new QPushButton("Create + track", createBox);
    createLayout->addWidget(newFileNameEdit);
    createLayout->addWidget(newFileContentEdit);
    createLayout->addWidget(createButton);

    connect(createButton, &QPushButton::clicked, this, &MainWindow::onCreateFile);

    // --- actions on the selected file ---
    auto* actionRow      = new QHBoxLayout;
    auto* stageButton    = new QPushButton("Stage selected", page);
    auto* stageAllButton = new QPushButton("Stage all modified", page);
    auto* refreshButton  = new QPushButton("Re-read from disk", page);
    auto* contentButton  = new QPushButton("Show content", page);
    actionRow->addWidget(stageButton);
    actionRow->addWidget(stageAllButton);
    actionRow->addWidget(refreshButton);
    actionRow->addWidget(contentButton);
    actionRow->addStretch();

    connect(stageButton,    &QPushButton::clicked, this, &MainWindow::onStageFile);
    connect(stageAllButton, &QPushButton::clicked, this, &MainWindow::onStageAll);
    connect(refreshButton,  &QPushButton::clicked, this, &MainWindow::onRefreshFile);
    connect(contentButton,  &QPushButton::clicked, this, &MainWindow::onShowContent);

    fileContentView = new QPlainTextEdit(page);
    fileContentView->setReadOnly(true);
    fileContentView->setFont(monospaceFont());
    fileContentView->setPlaceholderText("Select a file and press 'Show content'.");
    fileContentView->setMaximumHeight(140);

    // Feature 3 — stays visible while staged work has not been committed
    uncommittedLabel = new QLabel(page);
    uncommittedLabel->setStyleSheet("color: #e0a030; font-weight: bold;");
    uncommittedLabel->hide();

    layout->addWidget(new QLabel("Tracked files:", page));
    layout->addWidget(uncommittedLabel);
    layout->addWidget(filesTable, 1);
    layout->addLayout(actionRow);
    layout->addWidget(fileContentView);
    layout->addWidget(trackBox);
    layout->addWidget(createBox);

    const QList<QWidget*> needsRepo = {addButton, browseButton, createButton,
                                       stageButton, stageAllButton, refreshButton, contentButton};
    repoDependentWidgets.append(needsRepo);

    filesTable->setObjectName("filesTable");
    uncommittedLabel->setObjectName("uncommittedLabel");
    filePathEdit->setObjectName("filePathEdit");
    newFileNameEdit->setObjectName("newFileNameEdit");
    newFileContentEdit->setObjectName("newFileContentEdit");
    fileContentView->setObjectName("fileContentView");
    addButton->setObjectName("addFileButton");
    createButton->setObjectName("createFileButton");
    stageButton->setObjectName("stageButton");
    stageAllButton->setObjectName("stageAllButton");
    refreshButton->setObjectName("refreshFileButton");
    contentButton->setObjectName("showContentButton");

    return page;
}

QWidget* MainWindow::buildCommitsTab() {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // --- make a commit ---
    auto* commitBox  = new QGroupBox("Commit staged files", page);
    auto* commitForm = new QFormLayout(commitBox);
    authorEdit       = new QLineEdit(commitBox);
    messageEdit      = new QLineEdit(commitBox);
    authorEdit->setPlaceholderText("must contain at least one letter");
    messageEdit->setPlaceholderText("what changed?");

    auto* commitButton = new QPushButton("Commit", commitBox);
    commitForm->addRow("Author:",  authorEdit);
    commitForm->addRow("Message:", messageEdit);
    commitForm->addRow("", commitButton);

    connect(commitButton, &QPushButton::clicked, this, &MainWindow::onCommit);
    connect(messageEdit,  &QLineEdit::returnPressed, this, &MainWindow::onCommit);

    // --- history ---
    commitsTable = new QTableWidget(0, 5, page);
    commitsTable->setHorizontalHeaderLabels({"#", "Commit ID", "Author", "Message", "Timestamp"});
    // the message takes the slack; everything else is sized to fit its contents
    // so timestamps and ids are never cut off
    for (int column : {0, 1, 2, 4}) {
        commitsTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    commitsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    commitsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    commitsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    commitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // --- search ---
    auto* searchRow    = new QHBoxLayout;
    searchEdit         = new QLineEdit(page);
    searchEdit->setPlaceholderText("search commit summaries (empty shows everything)");
    auto* searchButton = new QPushButton("Search", page);
    searchRow->addWidget(searchEdit);
    searchRow->addWidget(searchButton);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchCommits);
    connect(searchEdit,   &QLineEdit::returnPressed, this, &MainWindow::onSearchCommits);

    // --- restore ---
    auto* restoreButton = new QPushButton("Restore selected file (Files tab) from selected commit", page);
    connect(restoreButton, &QPushButton::clicked, this, &MainWindow::onRestoreFile);

    layout->addWidget(commitBox);
    layout->addLayout(searchRow);
    layout->addWidget(new QLabel("History (newest last):", page));
    layout->addWidget(commitsTable, 1);
    layout->addWidget(restoreButton);

    const QList<QWidget*> needsRepo = {commitButton, searchButton, restoreButton};
    repoDependentWidgets.append(needsRepo);

    commitsTable->setObjectName("commitsTable");
    authorEdit->setObjectName("authorEdit");
    messageEdit->setObjectName("messageEdit");
    searchEdit->setObjectName("searchEdit");
    commitButton->setObjectName("commitButton");
    searchButton->setObjectName("searchButton");
    restoreButton->setObjectName("restoreButton");

    return page;
}

QWidget* MainWindow::buildDiffTab() {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* pickRow = new QHBoxLayout;
    diffCommitBox = new QComboBox(page);
    diffFileBox   = new QComboBox(page);
    diffFileBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    auto* diffButton = new QPushButton("Compute diff", page);

    pickRow->addWidget(new QLabel("Commit:", page));
    pickRow->addWidget(diffCommitBox);
    pickRow->addWidget(new QLabel("File:", page));
    pickRow->addWidget(diffFileBox, 1);
    pickRow->addWidget(diffButton);

    connect(diffButton, &QPushButton::clicked, this, &MainWindow::onComputeDiff);

    diffView = new QPlainTextEdit(page);
    diffView->setReadOnly(true);
    diffView->setFont(monospaceFont());
    diffView->setPlaceholderText("Pick a commit and a file, then press 'Compute diff'.\n"
                                 "Lines marked '-' are in the commit, '+' are in the working copy.");

    layout->addLayout(pickRow);
    layout->addWidget(diffView, 1);

    repoDependentWidgets.append(diffButton);

    diffCommitBox->setObjectName("diffCommitBox");
    diffFileBox->setObjectName("diffFileBox");
    diffButton->setObjectName("diffButton");
    diffView->setObjectName("diffView");

    return page;
}

QWidget* MainWindow::buildAnalyticsTab() {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* statsBox  = new QGroupBox("Repository statistics", page);
    auto* statsForm = new QFormLayout(statsBox);

    statCommitsLabel = new QLabel("0", statsBox);
    statFilesLabel   = new QLabel("0", statsBox);
    statStagedLabel  = new QLabel("0", statsBox);
    statMostLabel    = new QLabel("-", statsBox);

    statsForm->addRow("Commits:",            statCommitsLabel);
    statsForm->addRow("Tracked files:",      statFilesLabel);
    statsForm->addRow("Currently staged:",   statStagedLabel);
    statsForm->addRow("Most committed file:", statMostLabel);

    auto* refreshButton = new QPushButton("Refresh", page);
    connect(refreshButton, &QPushButton::clicked, this, [this] {
        refreshAnalytics();
        statusBar()->showMessage("Statistics refreshed.");
    });

    layout->addWidget(statsBox);
    layout->addWidget(refreshButton);
    layout->addStretch();

    statCommitsLabel->setObjectName("statCommitsLabel");
    statFilesLabel->setObjectName("statFilesLabel");
    statStagedLabel->setObjectName("statStagedLabel");
    statMostLabel->setObjectName("statMostLabel");
    refreshButton->setObjectName("statsRefreshButton");

    return page;
}

// ---------------------------------------------------------------------------
// Repository tab slots
// ---------------------------------------------------------------------------

void MainWindow::onInitRepository() {
    // creating a repository throws away whatever is open right now
    if (manager.isInitialized() && !confirmDiscardChanges()) return;

    const bool ok = manager.initRepository(toStd(repoNameEdit->text()),
                                           toStd(repoPathEdit->text()));
    report(ok);
    if (ok) {
        setRepoDependentEnabled(true);
        tabs->setCurrentIndex(1);   // straight on to the Files tab
    }
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onSaveRepository() {
    // Feature 4 — refuse to save while the form is invalid, rather than writing
    // a half-valid repository out to disk.
    if (!manager.isInitialized()) {
        QMessageBox::warning(this, "Save", "Create or load a repository first.");
        return;
    }
    if (saveFileEdit->text().trimmed().isEmpty()) {
        markField(saveFileEdit, "a data file name is required");
        QMessageBox::warning(this, "Save", "Enter a name for the data file first.");
        return;
    }
    markField(saveFileEdit, "");

    const QString file = saveFileEdit->text().trimmed();
    const bool ok = guarded("Saving the repository", [this, file] {
        return manager.saveRepository(toStd(file));
    });

    report(ok);
    if (!ok) return;

    lastDataFile = file;
    QSettings().setValue("lastDataFile", file);   // reopened on next startup
    setUnsavedChanges(false);
}

void MainWindow::onLoadRepository() {
    if (!confirmDiscardChanges()) return;

    const QString file = saveFileEdit->text().trimmed();
    const bool ok = guarded("Loading the repository", [this, file] {
        return manager.loadRepository(toStd(file));
    });

    report(ok);
    if (ok) {
        setRepoDependentEnabled(true);
        repoNameEdit->setText(toQ(manager.getRepositoryName()));
        repoPathEdit->setText(toQ(manager.getRepositoryPath()));
        lastDataFile = file;
        QSettings().setValue("lastDataFile", file);
    }
    refreshAll();
    if (ok) setUnsavedChanges(false);
}

// ---------------------------------------------------------------------------
// Files tab slots
// ---------------------------------------------------------------------------

void MainWindow::onBrowseForFile() {
    const QString chosen = QFileDialog::getOpenFileName(this, "Choose a file to track");
    if (chosen.isEmpty()) return;

    // A path relative to the repository reads better in the table (and matches
    // what the console front end stores), so shorten it when we can.
    const QDir repoDir(toQ(manager.getRepositoryPath()));
    const QString relative = repoDir.relativeFilePath(chosen);
    filePathEdit->setText(relative.startsWith("..") ? chosen : relative);
}

void MainWindow::onAddFile() {
    if (filePathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Track a file", "Choose a file first.");
        return;
    }
    const QString path = filePathEdit->text();
    const bool ok = guarded("Tracking the file", [this, path] {
        return manager.addFile(toStd(path));
    });

    report(ok);
    if (ok) filePathEdit->clear();
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onCreateFile() {
    if (newFileNameEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Create a file", "Give the new file a path.");
        return;
    }
    const QString path    = newFileNameEdit->text();
    const QString content = newFileContentEdit->toPlainText();
    const bool ok = guarded("Creating the file", [this, path, content] {
        return manager.createAndTrackFile(toStd(path), toStd(content));
    });

    report(ok);
    if (ok) {
        newFileNameEdit->clear();
        newFileContentEdit->clear();
    }
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onStageFile() {
    const QString path = selectedFilePath();
    if (path.isEmpty()) {
        QMessageBox::information(this, "Stage", "Select a file in the table first.");
        return;
    }
    const bool ok = manager.stageFile(toStd(path));
    report(ok);
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onStageAll() {
    const bool ok = manager.stageAllFiles();
    report(ok);
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onRefreshFile() {
    const QString path = selectedFilePath();
    if (path.isEmpty()) {
        QMessageBox::information(this, "Re-read", "Select a file in the table first.");
        return;
    }
    const bool ok = guarded("Re-reading the file", [this, path] {
        return manager.refreshFile(toStd(path));
    });

    report(ok);
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onShowContent() {
    const QString path = selectedFilePath();
    if (path.isEmpty()) {
        QMessageBox::information(this, "Show content", "Select a file in the table first.");
        return;
    }
    std::string content;
    const bool ok = guarded("Reading the file", [this, path, &content] {
        return manager.getFileContent(toStd(path), content);
    });

    if (ok) {
        fileContentView->setPlainText(toQ(content));
        statusBar()->showMessage(toQ(manager.lastMessage()));
    } else if (!manager.lastMessage().empty()) {
        report(false);
    }
}

// ---------------------------------------------------------------------------
// Commits tab slots
// ---------------------------------------------------------------------------

void MainWindow::onCommit() {
    const bool ok = manager.commitChanges(toStd(messageEdit->text()),
                                          toStd(authorEdit->text()));
    report(ok);
    if (ok) messageEdit->clear();   // the author usually stays the same
    refreshAll();
    if (ok) setUnsavedChanges(true);
}

void MainWindow::onSearchCommits() {
    // The manager's search returns matching summaries. The table is then rebuilt
    // from the real commits whose summary matched, so every column is filled and
    // a row picked out of a search result can still be used to restore a file.
    const std::vector<std::string> results = manager.searchCommits(toStd(searchEdit->text()));
    const std::set<std::string> matched(results.begin(), results.end());

    commitsTable->setRowCount(0);
    const auto& commits = manager.getCommits();

    for (std::size_t i = 0; i < commits.size(); ++i) {
        const Commit* commit = commits[i].get();
        if (!matched.contains(commit->getSummary())) continue;

        const int row = commitsTable->rowCount();
        commitsTable->insertRow(row);
        commitsTable->setItem(row, 0, cell(QString::number(i + 1)));
        commitsTable->setItem(row, 1, cell(toQ(commit->getCommitID())));
        commitsTable->setItem(row, 2, cell(toQ(commit->getAuthor())));
        commitsTable->setItem(row, 3, cell(toQ(commit->getMessage())));
        commitsTable->setItem(row, 4, cell(toQ(commit->getTimestamp())));
    }

    statusBar()->showMessage(QString("%1 commit(s) match '%2'.")
                                 .arg(commitsTable->rowCount())
                                 .arg(searchEdit->text()));
}

void MainWindow::onRestoreFile() {
    const QString commitId = selectedCommitId();
    const QString path     = selectedFilePath();

    if (commitId.isEmpty()) {
        QMessageBox::information(this, "Restore", "Select a commit in the table first.");
        return;
    }
    if (path.isEmpty()) {
        QMessageBox::information(this, "Restore",
                                 "Select the file to restore in the Files tab first.");
        return;
    }

    // Feature 6 — restoring overwrites the tracked copy, so confirm first
    const auto proceed = QMessageBox::warning(
        this, "Restore",
        QString("Restoring '%1' from commit %2 will overwrite the current "
                "changes to that file.\n\nContinue?").arg(path, commitId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (proceed != QMessageBox::Yes) return;

    if (!manager.restoreFile(toStd(commitId), toStd(path))) {
        report(false);
        return;
    }
    report(true);
    setUnsavedChanges(true);

    // Overwriting the working file is a separate, explicit decision — same as the
    // y/n question the console front end asks.
    const auto answer = QMessageBox::question(
        this, "Write to disk",
        QString("'%1' was restored from commit %2.\n\nWrite the restored content "
                "over the file on disk?").arg(path, commitId));

    if (answer == QMessageBox::Yes) {
        report(guarded("Writing the file", [this, path] {
            return manager.writeFileToDisk(toStd(path));
        }));
    }
    refreshAll();
}

// ---------------------------------------------------------------------------
// Diff tab slots
// ---------------------------------------------------------------------------

void MainWindow::onComputeDiff() {
    if (diffCommitBox->count() == 0 || diffFileBox->count() == 0) {
        QMessageBox::information(this, "Diff", "Commit a file first — there is nothing to compare.");
        return;
    }

    const QString commitId = diffCommitBox->currentData().toString();
    const QString path     = diffFileBox->currentText();

    std::string diff;
    if (manager.diffFileAgainstCommit(toStd(commitId), toStd(path), diff)) {
        diffView->setPlainText(toQ(diff));
        statusBar()->showMessage(toQ(manager.lastMessage()));
    } else {
        diffView->clear();
        report(false);
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers — the widgets are always redrawn from the model, never
// updated piecemeal, so what is on screen cannot drift out of sync with it.
// ---------------------------------------------------------------------------

void MainWindow::refreshAll() {
    refreshFilesTable();
    refreshCommitsTable();
    refreshDiffChoices();
    refreshAnalytics();
    refreshRepoSummary();
}

void MainWindow::refreshFilesTable() {
    const QString previous = selectedFilePath();

    filesTable->setRowCount(0);
    for (const TrackedFile& file : manager.getFiles()) {
        const int row = filesTable->rowCount();
        filesTable->insertRow(row);
        filesTable->setItem(row, 0, cell(toQ(file.getPath())));
        filesTable->setItem(row, 1, cell(toQ(statusToString(file.getStatus()))));
        filesTable->setItem(row, 2, cell(QString::number(file.getSize())));
    }

    // keep the user's selection across a refresh
    for (int row = 0; row < filesTable->rowCount(); ++row) {
        if (filesTable->item(row, 0)->text() == previous) {
            filesTable->selectRow(row);
            break;
        }
    }
}

void MainWindow::refreshCommitsTable() {
    const QString previous = selectedCommitId();

    commitsTable->setRowCount(0);
    const auto& commits = manager.getCommits();

    for (std::size_t i = 0; i < commits.size(); ++i) {
        const Commit* commit = commits[i].get();
        const int row = commitsTable->rowCount();
        commitsTable->insertRow(row);
        commitsTable->setItem(row, 0, cell(QString::number(i + 1)));
        commitsTable->setItem(row, 1, cell(toQ(commit->getCommitID())));
        commitsTable->setItem(row, 2, cell(toQ(commit->getAuthor())));
        commitsTable->setItem(row, 3, cell(toQ(commit->getMessage())));
        commitsTable->setItem(row, 4, cell(toQ(commit->getTimestamp())));
    }

    for (int row = 0; row < commitsTable->rowCount(); ++row) {
        if (commitsTable->item(row, 1) && commitsTable->item(row, 1)->text() == previous) {
            commitsTable->selectRow(row);
            break;
        }
    }
}

void MainWindow::refreshDiffChoices() {
    const QString previousCommit = diffCommitBox->currentData().toString();
    const QString previousFile   = diffFileBox->currentText();

    diffCommitBox->clear();
    const auto& commits = manager.getCommits();
    for (std::size_t i = 0; i < commits.size(); ++i) {
        const Commit* commit = commits[i].get();
        diffCommitBox->addItem(QString("#%1  %2  %3")
                                   .arg(i + 1)
                                   .arg(toQ(commit->getCommitID()),
                                        toQ(commit->getMessage())),
                               toQ(commit->getCommitID()));
    }

    diffFileBox->clear();
    for (const TrackedFile& file : manager.getFiles()) {
        diffFileBox->addItem(toQ(file.getPath()));
    }

    const int commitIndex = diffCommitBox->findData(previousCommit);
    if (commitIndex >= 0) diffCommitBox->setCurrentIndex(commitIndex);

    const int fileIndex = diffFileBox->findText(previousFile);
    if (fileIndex >= 0) diffFileBox->setCurrentIndex(fileIndex);
}

void MainWindow::refreshAnalytics() {
    statCommitsLabel->setText(QString::number(manager.getTotalCommits()));
    statFilesLabel->setText(QString::number(manager.getTrackedFileCount()));
    statStagedLabel->setText(QString::number(manager.getStagedCount()));
    statMostLabel->setText(toQ(manager.getMostModifiedFile()));
}

void MainWindow::refreshRepoSummary() {
    // Feature 3 — warn about staged files that have not been committed yet
    if (uncommittedLabel != nullptr) {
        const int staged = manager.getStagedCount();
        uncommittedLabel->setVisible(staged > 0);
        uncommittedLabel->setText(
            QString("Warning: %1 file(s) staged but not committed - use the Commits tab.")
                .arg(staged));
    }

    if (!manager.isInitialized()) {
        repoSummaryLabel->setText("<i>No repository open.</i>");
        return;
    }
    repoSummaryLabel->setText(QString("<b>%1</b> at <code>%2</code> - %3 file(s), %4 commit(s), %5 staged.")
                                  .arg(toQ(manager.getRepositoryName()),
                                       toQ(manager.getRepositoryPath()))
                                  .arg(manager.getTrackedFileCount())
                                  .arg(manager.getTotalCommits())
                                  .arg(manager.getStagedCount()));
}

void MainWindow::setRepoDependentEnabled(const bool enabled) {
    for (QWidget* widget : repoDependentWidgets) {
        widget->setEnabled(enabled);
    }
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

void MainWindow::report(const bool ok) {
    const QString message = toQ(manager.lastMessage());
    if (ok) {
        statusBar()->showMessage(message, 8000);
    } else {
        // a failure the user must see, not something to miss in the status bar
        statusBar()->showMessage(message, 8000);
        QMessageBox::warning(this, "MiniVCS", message);
    }
}

bool MainWindow::guarded(const QString& action, const std::function<bool()>& operation) {
    try {
        return operation();
    }
    catch (const std::exception& ex) {
        // Anything the file system throws at us — a disk error, a permissions
        // problem, a bad conversion — is reported here instead of terminating.
        QMessageBox::critical(this, "MiniVCS - " + action,
                              action + " failed.\n\n" + QString::fromUtf8(ex.what()));
        statusBar()->showMessage(action + " failed: " + QString::fromUtf8(ex.what()), 8000);
        return false;
    }
    catch (...) {
        QMessageBox::critical(this, "MiniVCS - " + action,
                              action + " failed for an unknown reason.");
        return false;
    }
}

void MainWindow::setUnsavedChanges(const bool changed) {
    unsavedChanges = changed;

    // the title bar carries the usual '*' marker for unsaved work
    QString title = "MiniVCS - C++ Version Control System";
    if (manager.isInitialized()) title += "  [" + toQ(manager.getRepositoryName()) + "]";
    if (changed) title += " *";
    setWindowTitle(title);

    refreshRepoSummary();
}

bool MainWindow::confirmDiscardChanges() {
    if (!unsavedChanges) return true;

    const auto answer = QMessageBox::question(
        this, "Unsaved changes",
        "You have unsaved changes.\nDo you want to save before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (answer == QMessageBox::Cancel) return false;

    if (answer == QMessageBox::Save) {
        onSaveRepository();
        // a failed save must not silently throw the work away
        return !unsavedChanges;
    }

    return true;   // discard
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscardChanges()) event->accept();
    else                         event->ignore();
}

void MainWindow::loadPreviousSession() {
    // The data file used last time is remembered between runs, so the previous
    // session's repository comes back automatically on startup.
    QSettings settings;
    const QString previous = settings.value("lastDataFile").toString();

    if (previous.isEmpty() || !QFileInfo::exists(previous)) return;

    const bool ok = guarded("Loading the previous session", [this, previous] {
        return manager.loadRepository(toStd(previous));
    });

    if (!ok) {
        statusBar()->showMessage("Could not reopen the previous repository - starting empty.");
        return;
    }

    lastDataFile = previous;
    saveFileEdit->setText(previous);
    repoNameEdit->setText(toQ(manager.getRepositoryName()));
    repoPathEdit->setText(toQ(manager.getRepositoryPath()));
    setRepoDependentEnabled(true);
    refreshAll();
    setUnsavedChanges(false);
    statusBar()->showMessage("Reopened " + toQ(manager.lastMessage()), 8000);
}

void MainWindow::markField(QLineEdit* field, const std::string& problem) {
    if (field == nullptr) return;

    if (problem.empty()) {
        field->setStyleSheet("");
        field->setToolTip("");
    } else {
        // invalid fields are highlighted in red as the user types
        field->setStyleSheet("background-color: #5c2b2b; border: 1px solid #b03030;");
        field->setToolTip(toQ(problem));
    }
}

void MainWindow::onValidateFields() {
    // An empty box is "not filled in yet" rather than "wrong", so it is left
    // alone until the user has actually typed something.
    const auto check = [this](QLineEdit* field, const std::string& problem) {
        if (field == nullptr) return;
        markField(field, field->text().isEmpty() ? "" : problem);
    };

    check(repoNameEdit, manager.checkRepoName(toStd(repoNameEdit->text())));
    check(repoPathEdit, manager.checkRepoPath(toStd(repoPathEdit->text())));
    check(authorEdit,   manager.checkAuthor(toStd(authorEdit->text())));
    check(messageEdit,  manager.checkMessage(toStd(messageEdit->text())));
}

QString MainWindow::selectedFilePath() const {
    const auto selected = filesTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return {};
    QTableWidgetItem* item = filesTable->item(selected.first().row(), 0);
    return item ? item->text() : QString();
}

QString MainWindow::selectedCommitId() const {
    const auto selected = commitsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return {};
    QTableWidgetItem* item = commitsTable->item(selected.first().row(), 1);
    return item ? item->text() : QString();
}
