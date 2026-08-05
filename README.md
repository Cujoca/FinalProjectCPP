# FinalProject — C++ Version Control System

A C++ project building out a version control system.

---

## Model Classes

### `Commit` — abstract base class
**Files:**`model/Commit.h`,`model/Commit.cpp`

The backbone of the commit system. Every commit stores four fields:`author`,`message`,
`timestamp`, and`commitID`. It's abstract —`displayCommit()`and`getSummary()`are pure
virtual, so concrete subclasses must implement them.

A few design notes:
- Two`Commit`s are considered **equal if and only if their`commitID`s match**, regardless of
  author, message, or anything else. This is enforced via an overloaded`operator==`.
-`operator<<`is overloaded and delegates to`getSummary()`, so you can stream any commit
  directly to`cout`.

---

### `StandardCommit` — extends `Commit`
**Files:**`model/StandardCommit.h`,`model/StandardCommit.cpp`

The concrete commit implementation. Adds`fileSnapshots`— a`map<string, string>`storing
a full snapshot of every tracked file's content at commit time (path → content).

`displayCommit()`prints the standard commit metadata followed by a list of snapshotted file
paths (content is intentionally omitted — nobody wants that dumped to the terminal).

`getSummary()`returns a compact one-liner in this format:
```
COMMIT-<id>-<N> files-<message>-<author>
```

TODO:`getSummary()`should eventually make use of the extra snapshot data rather than
behaving identically to the base class version.

---

### `TrackedFile`
**Files:**`model/TrackedFile.h`,`model/TrackedFile.cpp`

Represents a single file under version control. Holds the file's`path`,`content`, byte`size`,
and a`Status`tracking where it sits in the commit workflow.

**`Status`enum:**
-`Modified`— changed but not staged
-`Staged`— staged and ready to commit
-`Committed`— part of a commit
-`Error`— unrecognized state (also the fallback for`statusFromString`on unknown input)

A freshly constructed`TrackedFile`starts as`Modified`, with`size`derived automatically
from the initial content.

Content can be updated via`setContent()`or`updateContent()`— both currently do the same
thing (replace content and recalculate size). The separation exists in case one of them is
eventually reworked to accept a delta rather than the full new content.

`statusToString`/`statusFromString`handle round-tripping the enum to and from strings.
`operator<<`is overloaded for`Status`as a convenience wrapper around`statusToString`.

---

### `Validator`
**Files:**`model/Validator.h`,`model/Validator.cpp`

Input validation. Every value the user types is checked here before the model
stores it, and each check returns `expected<string, Error>` — the cleaned-up
value on success, an `Error` on failure — so the caller gets back a usable
string and never has to re-trim it.

| Method | Rule |
|--------|------|
| `validateRepoName` | 3–20 chars after trimming, at least one letter |
| `validateRepoPath` | Windows absolute (`C:\work\repo`) or relative/POSIX (`.`, `data/repo`); rejects `* ? " < > |` |
| `validateCommitID` | non-empty, letters/digits/dashes, max 40 chars |
| `validateMessage`  | non-empty after trimming, max 200 chars |
| `validateAuthor`   | non-empty after trimming, at least one letter, max 50 chars |

Names and paths are trimmed **before** the length rules run, so `"  ab  "` is
correctly rejected as too short instead of passing on its padding.

---

### `Repository`
**Files:**`model/Repository.h`,`model/Repository.cpp`

The backend. Holds the repository name and path, a `vector<TrackedFile>` of the
working files and a `vector<unique_ptr<Commit>>` of history, and implements the
core verbs: `addFile`, `stageFile`, `commitChanges`, `restoreFile`,
`refreshFile`, plus `writeFile` for pushing content back to disk.

Three things worth knowing:
- **Paths are relative to the repository root, the way git does it.** A file is
  identified by its path relative to the repository folder, and that relative
  form is what gets stored, displayed, snapshotted and saved. A repository at
  `C:/Users/me/Desktop` tracking `notes.txt` therefore reads and writes
  `C:/Users/me/Desktop/notes.txt`. `resolvePath()` turns the stored form into a
  real disk location; `toRepoRelative()` does the reverse, shortening an absolute
  path that falls inside the repository (one pointing outside is kept absolute,
  so files elsewhere on the machine can still be tracked). Creating a file makes
  any missing folders along the way.
- **Commits are full snapshots.** `commitChanges` starts from the previous
  commit's snapshot and overlays whatever is staged now, so a file committed in
  commit 1 is still present in commit 5 and `restoreFile` can always reach it.
- **It does no I/O of its own.** Failures come back as `false` and the *reason*
  is worked out one layer up in `RepositoryManager`, which keeps every
  user-facing message in the view layer where it belongs.

---

### `DataManager`
**Files:**`model/DataManager.h`,`model/DataManager.cpp`

Saves a `Repository` to a plain-text file and loads it back — repo metadata,
every tracked file with its status and content, and every commit with its
snapshot map. Multi-line content is written as a length-prefixed block so
newlines inside a file can't be confused with the record separators.

A load parses the whole file into local containers first and only hands them to
the repository once it has succeeded, so a truncated or hand-edited save file is
reported as a failed load and **leaves the in-memory repository untouched**
rather than half-overwriting it.

---

### `DiffEngine`
**Files:**`model/DiffEngine.h`,`model/DiffEngine.cpp`

Line-by-line diff. Shared lines are found with a longest-common-subsequence
table, then the two versions are walked in step and each line is emitted as
context, `-` (only in the old version) or `+` (only in the new one), followed by
an added/removed tally. Handles LF and CRLF identically, and falls back to a
plain side-by-side dump for inputs too large to run the quadratic table on.

---

### `AnalyticsEngine<T>` — template
**Files:**`model/Analytics.h`

Small template over a container: `computeTotalCommits`, `computeTrackedFilesCount`
and `computeMostModifiedFiles` (which `dynamic_cast`s each `Commit` to
`StandardCommit` to reach its snapshot map and counts how often each path
appears). Instantiated twice in `RepositoryManager` — once over
`vector<unique_ptr<Commit>>`, once over `vector<TrackedFile>`. Everything is
taken by `const&`, since a vector of `unique_ptr` cannot be copied.

---

## Controller Layer

### `RepositoryManager`
**Files:**`controller/RepositoryManager.h`,`controller/RepositoryManager.cpp`

The bridge, and the only class `main.cpp` talks to. It is where the three layers
actually meet:

1. **Validates** raw user input with `Validator` before the model sees it.
2. **Delegates** the work to `Repository` / `DataManager` / `DiffEngine` /
   `AnalyticsEngine`.
3. **Explains** the outcome. Since the model's methods only return `bool`, the
   manager works out the specific reason ("already tracked", "cannot open",
   "nothing staged", "no commit with id …") and stores it in `lastMessage()`,
   which the app hands straight to the view. That is why a failed operation in
   the GUI always says *why* it failed.

---

## View / GUI Layer

### `ConsoleView`
**Files:** `view/ConsoleView.h`, `view/ConsoleView.cpp` — *Bao Vo*

The presentation layer — everything the user sees and types goes through here, and
nothing else does. `ConsoleView` renders model objects (the menu, repository status,
the commit log, commit details, diffs, search results, statistics) and reads raw
input; it holds **no business logic and never mutates model state**. Keeping all
I/O in one class means the rest of the program can run and be tested without a
live terminal.

The commit-rendering methods take the abstract `Commit`, not `StandardCommit`, so
the view can display the repository's `vector<unique_ptr<Commit>>` history
directly and would keep working if a second commit type were added later.

The two `static` formatting helpers — `formatStatusLine()` and
`formatCommitSummary()` — are pure (string in, string out, no I/O), so they are unit
tested directly in `tests/test_main.cpp`.

---

### `MainWindow` — Qt Widgets GUI
**Files:** `view/MainWindow.h`, `view/MainWindow.cpp`, `view/gui_main.cpp` — *Bao Vo*

The graphical front end: a `QMainWindow` wrapping a five-tab `QTabWidget`.

| Tab | Contents |
|-----|----------|
| **Repository** | create a repository (name + path, with a folder browser), save / load a data file, live summary line |
| **Files** | table of tracked files (path, status, size); **Add**, Browse, Create + track, **Stage**, Stage all, Re-read from disk, Show content |
| **Commits** | author + message + **Commit**; full history table (#, id, author, message, timestamp); search box; **Restore** |
| **Diff** | commit picker + file picker → monospaced diff of the commit's snapshot vs. the working copy |
| **Analytics** | commits, tracked files, staged count, most-committed file |

It obeys the same rule as `ConsoleView`: **all** widgets and dialogs live here and
nothing else does. Each slot reads its widgets, calls exactly one
`RepositoryManager` method, and shows the manager's own message in the status
bar (successes) or a `QMessageBox` (failures). The widget tree is always redrawn
from the model rather than patched in place, so the display cannot drift out of
sync with the repository.

Behaviours worth calling out:
- Everything except **Load** is disabled until a repository exists, so the
  buttons can't be pressed into an error state.
- **Exception safety.** Every slot that touches the file system runs through
  `guarded()`, which wraps the call in `try` / `catch (const std::exception&)`
  and turns anything that escapes into a `QMessageBox::critical` instead of
  letting it terminate the program.
- **The previous session is restored on startup.** The data file used last time
  is remembered in `QSettings`, so reopening the app brings back the repository
  you were working on. A missing or corrupt file is reported and the app simply
  starts empty.
- **Unsaved work is tracked.** The title bar carries a `*`, and closing the
  window (or creating/loading another repository) prompts Save / Discard /
  Cancel rather than silently throwing the work away.

### Enhanced validation features

The specification asks for at least three of six; all six are implemented:

| # | Feature | How |
|---|---------|-----|
| 1 | Real-time validation | `onValidateFields()` re-checks name, path, author and message on every keystroke and paints an invalid field red with the reason in its tooltip |
| 2 | Duplicate detection | tracking a file that is already tracked is refused and explained |
| 3 | Uncommitted changes warning | a banner on the Files tab stays visible while files are staged but not committed |
| 4 | Auto-save validation | saving is refused, and the field flagged, when the repository or the data file name is not valid |
| 5 | Unsaved changes detection | `closeEvent()` prompts "You have unsaved changes. Do you want to save before continuing?" |
| 6 | Restore confirmation | restoring warns that current changes will be overwritten before it proceeds |

The rules themselves stay in the model: the view asks
`RepositoryManager::checkRepoName()` and friends, which delegate to `Validator`,
so the GUI highlights fields without knowing any of the rules.

Because both front ends sit on the identical controller, **adding the GUI
required no change whatsoever to the model or controller layers**.

---

## Application / Integration Layer

### `main.cpp` — *Bao Vo*

The entry point that wires the finished pieces together into a runnable program.
It runs **MiniVCS**, an interactive version control workflow. The whole app is
one class, `MiniVCSApp`, holding exactly two members — a `ConsoleView` and a
`RepositoryManager` — and every menu handler is the same three steps: prompt
through the view, call one manager method, hand the manager's message back to
the view.

```
view/ConsoleView                         <- all terminal I/O
      |
controller/RepositoryManager             <- validation, orchestration, messages
      |
model/Repository, DataManager, DiffEngine, AnalyticsEngine
model/TrackedFile, Commit, StandardCommit, Validator
```

The menu:

| # | Action | Exercises |
|---|--------|-----------|
| 1 | Track an existing file | `Repository::addFile`, `Validator::validateRepoPath` |
| 2 | Create + track a new file | `Repository::writeFile` + `addFile` |
| 3 | Stage file(s) (`all` supported) | `Repository::stageFile` |
| 4 | Show status | `TrackedFile::Status`, `ConsoleView::formatStatusLine` |
| 5 | Commit staged files | `StandardCommit`, `validateAuthor` / `validateMessage` |
| 6 | Show commit log | `Commit::getSummary` (polymorphic) |
| 7 | Show commit details | `Commit::displayCommit` (polymorphic) |
| 8 | Show file content | `TrackedFile::getContent` |
| 9 | Diff file vs commit | `DiffEngine::computeDiff` |
| 10 | Restore file from commit | `Repository::restoreFile` (+ optional write to disk) |
| 11 | Re-read file from disk | `Repository::refreshFile` |
| 12 | Search commits | `RepositoryManager::searchCommits` |
| 13 | Repository statistics | `AnalyticsEngine` |
| 14 | Save repository | `DataManager::saveData` |
| 15 | Load repository | `DataManager::loadData` |

**File paths are relative to the repository folder.** With the repository set to
`C:/Users/me/Desktop`, typing `notes.txt` creates or tracks
`C:/Users/me/Desktop/notes.txt` — the same rule git uses. Absolute paths are
accepted too, and are shortened automatically when they fall inside the
repository. (The save/load data file is the one exception: it is relative to
wherever you launched the program, because loading has to work before any
repository exists.)

Commits can be referred to either by their id or by the `#N` row number shown in
the log. Options 9–11 are what make the VCS behaviour real: edit a tracked file
in any editor, re-read it (11), see exactly what changed against any commit (9),
and roll it back (10).

**Integration note:** the repository is no longer kept in memory by `main.cpp` —
it is Omer's `Repository`, reached through `RepositoryManager`, and persisted
with `DataManager`. `main.cpp` contains no business logic and no file I/O of its
own; it only routes between the view and the controller.

---

## Building & Running

The project uses **CMake** (C++23) and builds **two front ends** over one shared
backend:

| Target | What it is | Needs Qt? |
|--------|-----------|-----------|
| `FinalProjectCPP` | console application | no |
| `TestRunner` | backend + console test suite | no |
| `MiniVCSGui` | Qt Widgets application | yes |
| `GuiTestRunner` | functional tests that drive the real widgets | yes |

The Qt targets are **optional**: if Qt6 isn't found, CMake prints a note, skips
them, and the console app and tests build exactly as before.

### Console only (no Qt required)

```bash
cmake -S . -B build
cmake --build build
./target/FinalProjectCPP        # (target/Debug/FinalProjectCPP.exe with the VS/MSVC generator)
```

### With the Qt GUI

Point CMake at your Qt kit. The kit installed for this project is MinGW-based,
so it needs the MinGW generator and toolchain rather than MSVC:

```bash
cmake -S . -B build-qt -G "MinGW Makefiles" \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-qt
./target/MiniVCSGui
```

Add `C:\Qt\6.8.3\mingw_64\bin` and `C:\Qt\Tools\mingw1310_64\bin` to `PATH` (or
just open the project in Qt Creator, which does it for you) so the Qt DLLs are
found at run time.

Binaries are written to `target/`, and the console test binary to `tests/`.

---

## target/ vs tests/

-**target/**— where the main application binary (`FinalProjectCPP`) is output when you
  build the project normally.
-**tests/**— contains the test suite (`test_main.cpp`) and the`TestRunner`binary built
  from it. Tests are hand-rolled with a simple`CHECK`/`SECTION`harness. Output is
  written to`tests/test_output.txt`when run via the`RunTests`CMake target.

The suite covers every class in the project:

| Group | What it checks |
|-------|----------------|
| `TrackedFile`, `StandardCommit` | construction, getters/setters, status round-tripping, snapshots, summaries |
| `Validator` | all five validators, including trimming and the boundary lengths |
| `ConsoleView` | the pure formatting helpers |
| `DiffEngine` | added / removed / context lines, CRLF handling, identical input |
| `Repository` | add, stage, commit, restore, refresh, lookups, and that commits are full snapshots |
| `DataManager` | save/load round-trip, and that a corrupt file fails without wiping the repository |
| `AnalyticsEngine` | commit and file counts, most-committed file |
| `RepositoryManager` | that each failure produces the right explanation |
| End-to-end | track → stage → commit → edit → diff → restore → save → load in one flow, plus the view rendering live model objects |
| Scale & edge cases | 300 tracked files, 2000 commits, a ~1 MB file, empty files, empty repositories, a file deleted after staging, and special characters in a search |

Backend tests need real files, so they create a scratch directory
(`tests/tmp_test_files/`) and delete it again when the suite finishes.

### GUI tests

`tests/gui_test.cpp` builds into `GuiTestRunner` (Qt required) and tests the
window for real: it looks widgets up by object name, types into them, **clicks
the actual buttons**, and asserts on what the tables, combo boxes and labels then
display. A window that compiled but had a button wired to the wrong slot would
fail here.

It also covers the validation features: the red highlighting as text is typed,
the uncommitted-changes banner appearing and clearing, the `*` unsaved marker,
saving being refused with a blank file name, the session being restored on
startup, and a corrupt or missing saved session leaving the app usable.

```bash
cmake --build build-qt --target RunGuiTests    # -> tests/gui_test_output.txt
```

Only success paths are clicked, on purpose: a failed operation opens a modal
`QMessageBox`, which would block a non-interactive run. The failure messages are
already covered by the `RepositoryManager` tests.

Set `MINIVCS_GUI_SHOTS=<dir>` to have the run also save a PNG of each populated
tab — handy for the project write-up:

```bash
MINIVCS_GUI_SHOTS=docs/screenshots ./target/GuiTestRunner
```

Use the normal platform for this, not `QT_QPA_PLATFORM=offscreen` — the
offscreen plugin renders without fonts, so text comes out as empty boxes.

---

## Team & Task Division

| Member | Responsibilities |
|--------|------------------|
| Andrei Cojocaru | Model layer: `Commit`, `StandardCommit`, `TrackedFile`, `DiffEngine`, `Validator` |
| Omer Ozkaya | `Repository`, `RepositoryManager`, `DataManager`, `AnalyticsEngine` |
| Bao Vo | GUI (`view/MainWindow` Qt front end + `view/ConsoleView`), `main.cpp`, `CMakeLists.txt`, `README.md`, testing, integration |
