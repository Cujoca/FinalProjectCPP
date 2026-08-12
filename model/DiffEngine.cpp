#include "DiffEngine.h"
#include <iostream>

string DiffEngine::computeDiff(const string oldContent, const string newContent) {

    string outputResult = "";

    // if same content, no diff
    if (oldContent == newContent) {


        
        return "No differences found";
    }

    // basic version for now
    // later we can make line by line diff
    outputResult += "Old Content:\n";
    outputResult += oldContent;

    outputResult += "\n\nNew Content:\n";
    outputResult += newContent;

    return outputResult;
}



// Call the computeDiff, assing it to variable, then call this with it
void DiffEngine::displayDiff(const string diffText) {




    // simple console display for now
    cout << diffText << endl;
}