# MiniVCS — Manual Test Plan

**CST8219 – C++ Programming — Mini Version Control System**

Follow the steps in order: each one builds on the previous. Tick the result box
as you go. The whole run takes about 20 minutes.

Everything is done from the project folder:

```
C:\Users\pc\vsc\FinalProjectCPP
```

---

## Part 0 — Build and launch

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 0.1 | Close any running MiniVCS window | — | |
| 0.2 | Run `.\build-gui.bat` | Build finishes with no errors, MiniVCS opens | |
| 0.3 | Look at the window title | `MiniVCS - C++ Version Control System` | |
| 0.4 | Look at the tabs | Five tabs: **Repository │ Files │ Commits │ Diff │ Analytics** | |
| 0.5 | Look at the status bar | "Create or load a repository to begin." | |

> If step 0.2 says a file is locked, a MiniVCS window is still open somewhere.

**Prepare a test folder** — make a new empty folder on your Desktop called
`vcsdemo`. All file tests will use it.

---

## Part 1 — Repository tab: validation (Feature 1, real-time validation)

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 1.1 | In **Name**, type `ab` | Box turns **red**; hover shows "the name is too short" | |
| 1.2 | Clear it and type `12345` | Still red; tooltip says "must contain at least one letter" | |
| 1.3 | Type a 51+ character name | Red; tooltip says "too long" | |
| 1.4 | Type `demo-repo` | Red highlight **disappears** | |
| 1.5 | In **Path**, type `bad<name>` | Path box turns red ("the path is not valid") | |
| 1.6 | Click **Browse…**, pick your `vcsdemo` folder | Path fills in, red clears | |
| 1.7 | Click **Initialize** | Status bar: `Repository 'demo-repo' initialized at …vcsdemo.` and the app jumps to the **Files** tab | |
| 1.8 | Look at the title bar | Now shows `[demo-repo] *` — the `*` means unsaved work (Feature 5) | |
| 1.9 | Go back to **Repository** tab | Summary line reads `demo-repo at …vcsdemo - 0 file(s), 0 commit(s), 0 staged.` | |

---

## Part 2 — Files tab: tracking files

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 2.1 | Before creating a repo the buttons were greyed out — confirm they are now **enabled** | Add / Stage / Create + track all clickable | |
| 2.2 | Under "Create and track a new file", type name `notes.txt`, content `first line` | — | |
| 2.3 | Click **Create + track** | Status bar: `Created and now tracking 'notes.txt' at …vcsdemo/notes.txt.` | |
| 2.4 | Open your `vcsdemo` folder in Explorer | **`notes.txt` really exists there** (this is the git-style path rule) | |
| 2.5 | Look at the table | One row: `notes.txt` │ `Modified` │ `11` | |
| 2.6 | Try **Create + track** again with the same name `notes.txt` | Warning dialog: "already exists — use 'Track a file' instead" (Feature 2) | |
| 2.7 | Type name `docs/readme.txt`, content `nested` → **Create + track** | Succeeds; a `docs` folder is created inside `vcsdemo` automatically | |
| 2.8 | Outside the app, make a file `vcsdemo\extra.txt` in Notepad with any text | — | |
| 2.9 | In "Track an existing file" type `extra.txt` → **Add** | Status: `Tracking 'extra.txt' (Modified).` | |
| 2.10 | Click **Add** again with `extra.txt` | Warning: `'extra.txt' is already tracked.` (duplicate detection) | |
| 2.11 | Type `ghost.txt` → **Add** | Warning: `Cannot open …ghost.txt — check the file exists…` | |
| 2.12 | Select `notes.txt` in the table, click **Show content** | Panel below shows `first line` | |

---

## Part 3 — Staging (Feature 3, uncommitted warning)

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 3.1 | Click **Stage selected** without selecting a row | Info dialog: "Select a file in the table first." | |
| 3.2 | Select `notes.txt`, click **Stage selected** | Status becomes `Staged` | |
| 3.3 | Look above the table | **Orange warning**: "1 file(s) staged but not committed" | |
| 3.4 | Click **Stage selected** on `notes.txt` again | Warning: `'notes.txt' is already staged.` | |
| 3.5 | Click **Stage all modified** | Remaining files become `Staged`; warning now says 3 | |
| 3.6 | Click **Stage all modified** again | Warning: "No modified files to stage." | |

---

## Part 4 — Commits tab

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 4.1 | In **Author**, type `123` | Box turns red — "must contain at least one letter" | |
| 4.2 | Type `Bao Vo` | Red clears | |
| 4.3 | Leave **Message** empty, click **Commit** | Warning: `Message rejected: the message cannot be empty.` | |
| 4.4 | Message `first commit` → **Commit** | Status: `Created commit 1 — COMMIT-1-3 files-first commit-Bao Vo.` | |
| 4.5 | Look at the history table | One row: `#1 │ 1 │ Bao Vo │ first commit │ <timestamp>` | |
| 4.6 | Go to **Files** tab | All files now `Committed`; the orange warning is **gone** | |
| 4.7 | Back on **Commits**, click **Commit** again | Warning: "Nothing staged to commit — stage a file first." | |

---

## Part 5 — Diff tab

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 5.1 | Open `vcsdemo\notes.txt` in Notepad, add a second line `second line`, save | — | |
| 5.2 | In the app: **Files** tab → select `notes.txt` → **Re-read from disk** | Status returns to `Modified` | |
| 5.3 | Go to **Diff** tab. Commit = `#1 …`, File = `notes.txt` | — | |
| 5.4 | Click **Compute diff** | Shows the committed version under `Old Content:` and the edited version under `New Content:`, so `second line` appears only in the new one | |
| 5.5 | Select `docs/readme.txt` (unchanged) → **Compute diff** | `No differences found` | |

---

## Part 6 — Restore (Feature 6, restore confirmation)

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 6.1 | **Commits** tab → click **Restore…** without selecting a commit | Info: "Select a commit in the table first." | |
| 6.2 | Select commit row #1, make sure `notes.txt` is selected on the Files tab, click **Restore…** | Warning: "Restoring 'notes.txt' … will overwrite the current changes. Continue?" | |
| 6.3 | Click **No** | Nothing changes | |
| 6.4 | Repeat and click **Yes** | Status: `Restored 'notes.txt' from commit 1 (now Modified).` | |
| 6.5 | Second dialog asks "Write the restored content over the file on disk?" → **Yes** | Status: `Wrote 'notes.txt' to disk (11 bytes).` | |
| 6.6 | Open `vcsdemo\notes.txt` in Notepad | The `second line` is **gone** — rolled back | |

---

## Part 7 — Search and Analytics

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 7.1 | **Commits** tab, search box: type `first` → **Search** | 1 row shown, all columns filled | |
| 7.2 | Search `zzzzz` → **Search** | Table empty; status "0 commit(s) match" | |
| 7.3 | Search `(){}[]*?` → **Search** | No crash, empty result | |
| 7.4 | Clear the box → **Search** | All commits come back | |
| 7.5 | **Analytics** tab | Commits `1`, Tracked files `3`, Staged `0`, Most committed shows a file name | |

---

## Part 8 — Persistence (Features 4 & 5)

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 8.1 | **Repository** tab → clear the **Data file** box → **Save repository** | Box turns red + warning "Enter a name for the data file first." (Feature 4) | |
| 8.2 | Type `vcsdemo.dat` → **Save repository** | Status: `Saved 3 file(s) and 1 commit(s) to 'vcsdemo.dat'.` | |
| 8.3 | Look at the title bar | The `*` is **gone** | |
| 8.4 | Make a change (stage a file) then click the window's **X** | Prompt: "You have unsaved changes. Do you want to save before continuing?" | |
| 8.5 | Click **Cancel** | Window stays open | |
| 8.6 | Click **X** again → **Discard** | App closes | |
| 8.7 | Launch `target\MiniVCSGui.exe` again | **The repository is already loaded** — files and commits are back (load on startup) | |
| 8.8 | Check the title bar | No `*` — a freshly restored session has no unsaved work | |

---

## Part 9 — Error handling / edge cases

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 9.1 | Repository tab → Data file `nosuch.dat` → **Load repository** | Warning: "Could not load 'nosuch.dat' — the file is missing or not a valid save file." App keeps working | |
| 9.2 | In Notepad create `vcsdemo\broken.dat` containing just `abc` and save. Load it | Same graceful warning, existing repository untouched | |
| 9.3 | Diff tab: press **Compute diff** with nothing committed (fresh repo) | Info dialog, no crash | |
| 9.4 | Click **Commit** rapidly 5 times in a row | Only the first works; the rest say "Nothing staged" — no duplicate commits | |
| 9.5 | Create a file, stage it, delete it in Explorer, then **Re-read from disk** | Warning "Cannot re-read…", app stays alive | |

---

## Part 10 — Console version (same backend)

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 10.1 | Run `.\target\FinalProjectCPP.exe` | Menu with options 1–15 and 0 | |
| 10.2 | Name `ab` | "Repository name rejected: the name is too short." | |
| 10.3 | Name `console-repo`, path `vcsdemo` | Repository initialized | |
| 10.4 | `2` → `hello.txt` → `hi there` | Created and tracked | |
| 10.5 | `3` → `all`, then `5` → author `Bao`, message `console commit` | Commit created | |
| 10.6 | `6` | Commit log lists the commit | |
| 10.7 | `13` | Statistics printed | |
| 10.8 | `abc` (letters instead of a number) | "Unknown option" — no crash | |
| 10.9 | `0` | "Closing repository … Goodbye!" | |

---

## Part 11 — Automated suites

| # | Action | Expected result | ✔ / ✘ |
|---|--------|-----------------|-------|
| 11.1 | `.\tests\TestRunner.exe` | `Results: 329 passed, 0 failed` | |
| 11.2 | `.\target\GuiTestRunner.exe` | `Results: 62 passed, 0 failed` | |

---

## Result summary

| Part | Area | Pass / Fail | Notes |
|------|------|-------------|-------|
| 0 | Build & launch | | |
| 1 | Repository validation | | |
| 2 | File tracking | | |
| 3 | Staging | | |
| 4 | Commits | | |
| 5 | Diff | | |
| 6 | Restore | | |
| 7 | Search & analytics | | |
| 8 | Persistence | | |
| 9 | Error handling | | |
| 10 | Console version | | |
| 11 | Automated suites | | |

Tested by: ______________________  Date: ______________
