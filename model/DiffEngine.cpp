#include "DiffEngine.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

// Splits a blob of file content into individual lines, tolerating both LF and
// CRLF endings so a file authored on Windows diffs the same as one from Linux.
vector<string> splitLines(const string& text) {
    vector<string> lines;
    istringstream stream(text);
    string line;

    while (getline(stream, line)) {

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        lines.push_back(line);
    }

    return lines;
}

// Old/new dumped side by side. Used as a fallback for inputs too large to run
// the quadratic line diff on.
string plainDiff(const string& oldContent, const string& newContent) {
    string outputResult = "";

    outputResult += "Old Content:\n";
    outputResult += oldContent;

    outputResult += "\n\nNew Content:\n";
    outputResult += newContent;

    return outputResult;
}

// Guard on the size of the LCS table so a huge file can't exhaust memory.
const size_t MAX_DIFF_CELLS = 4000000;

} // namespace

/* Line-by-line diff.
 *
 * Lines shared by both versions are found with a longest-common-subsequence
 * table, then the two versions are walked in step: a line only in the old
 * version is marked '-', a line only in the new version '+', and unchanged
 * lines are printed with no marker.
 */
string DiffEngine::computeDiff(const string oldContent, const string newContent) {

    // if same content, no diff
    if (oldContent == newContent) {
        return "No differences found";
    }

    const vector<string> oldLines = splitLines(oldContent);
    const vector<string> newLines = splitLines(newContent);

    const size_t oldCount = oldLines.size();
    const size_t newCount = newLines.size();

    if (oldCount * newCount > MAX_DIFF_CELLS) {
        return plainDiff(oldContent, newContent);
    }

    // lcs[i][j] = length of the longest common subsequence of oldLines[i..] and newLines[j..]
    vector<vector<int>> lcs(oldCount + 1, vector<int>(newCount + 1, 0));

    for (size_t i = oldCount; i-- > 0; ) {

        for (size_t j = newCount; j-- > 0; ) {

            if (oldLines[i] == newLines[j]) {
                lcs[i][j] = lcs[i + 1][j + 1] + 1;
            }
            else {
                lcs[i][j] = max(lcs[i + 1][j], lcs[i][j + 1]);
            }
        }
    }

    ostringstream outputResult;

    size_t i = 0;
    size_t j = 0;
    int added = 0;
    int removed = 0;

    while (i < oldCount && j < newCount) {

        if (oldLines[i] == newLines[j]) {
            outputResult << "    " << oldLines[i] << "\n";
            i++;
            j++;
        }
        else if (lcs[i + 1][j] >= lcs[i][j + 1]) {
            outputResult << "  - " << oldLines[i] << "\n";
            i++;
            removed++;
        }
        else {
            outputResult << "  + " << newLines[j] << "\n";
            j++;
            added++;
        }
    }

    // whatever is left over on either side is a pure removal / pure addition
    while (i < oldCount) {
        outputResult << "  - " << oldLines[i] << "\n";
        i++;
        removed++;
    }

    while (j < newCount) {
        outputResult << "  + " << newLines[j] << "\n";
        j++;
        added++;
    }

    outputResult << "(" << added << " line(s) added, " << removed << " line(s) removed)";

    return outputResult.str();
}



// Call the computeDiff, assing it to variable, then call this with it
void DiffEngine::displayDiff(const string diffText) {




    // simple console display for now
    cout << diffText << endl;
}
