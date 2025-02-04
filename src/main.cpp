// This file was developed by Thomas Müller <contact@tom94.net>.
// It is published under the GPLv3 License; see the LICENSE file.

#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

const string ANSI_SAVE_CURSOR = "\033[s";
const string ANSI_RESTORE_CURSOR = "\033[u";
const string ANSI_GRAY = "\033[38;5;243m";
const string ANSI_RESET = "\033[0m";
const string ANSI_CORRECT = "\033[38;5;15m";
const string ANSI_INCORRECT = "\033[38;5;9m";
const string ANSI_INCORRECT_WHITESPACE = "\033[41m";
const string ANSI_CLEAR_LINE = "\r\033[2K";
const string ANSI_MOVE_CURSOR_TO_BEGINNING_OF_LINE = "\r\033[G";

string moveCursorUp(int n) { return "\033[" + to_string(n) + "A"; }
string moveCursorDown(int n) { return "\033[" + to_string(n) + "B"; }
string moveCursorRight(int n) { return "\033[" + to_string(n) + "C"; }
string moveCursorLeft(int n) { return "\033[" + to_string(n) + "D"; }

string displayChar(char c, bool leading = false) {
	if (leading && c == '\t') {
		return "    ";
	}

	return string(1, c);
}

size_t drawState(const vector<string>& targetLines, const string& userInput) {
	// Compute cumulative offsets for each line within the displayed block.
	vector<size_t> offsets(targetLines.size());
	size_t cum = 0;
	for (size_t i = 0; i < targetLines.size(); i++) {
		offsets[i] = cum;
		cum += targetLines[i].size() + 1; // +1 for the newline
	}

	// Update each displayed line without clearing the entire screen.
	for (size_t i = 0; i < targetLines.size(); i++) {
		// Compute leading whitespace count for the current line.
		int indent = 0;
		while (indent < (int)targetLines[i].size() && (targetLines[i][indent] == ' ' || targetLines[i][indent] == '\t')) {
			indent++;
		}

		cout << ANSI_CLEAR_LINE;
		int lineStart = offsets[i];
		int lineEnd = offsets[i] + targetLines[i].size();
		for (int j = lineStart; j < lineEnd; j++) {
			int local_j = j - offsets[i];
			bool isLeading = (local_j < indent);
			if (j < (int)userInput.size()) {
				if (userInput[j] == targetLines[i][local_j]) {
					cout << ANSI_CORRECT << displayChar(targetLines[i][local_j], isLeading) << ANSI_RESET;
				} else {
					if (isspace(userInput[j])) {
						cout << ANSI_INCORRECT_WHITESPACE << displayChar(userInput[j], isLeading) << ANSI_RESET;
					} else {
						cout << ANSI_INCORRECT << displayChar(userInput[j], isLeading) << ANSI_RESET;
					}
				}
			} else {
				cout << ANSI_GRAY << displayChar(targetLines[i][local_j], isLeading) << ANSI_RESET;
			}
		}

		if (i < targetLines.size() - 1) {
			cout << "\n";
		}
	}

	cout.flush();
	size_t totalExpected = offsets.back() + targetLines.back().size();
	return totalExpected;
}

void moveCursor(const string& userInput) {
	// Determine the cursor's current row and column based on userInput (accounting for tab width).
	int currentLine = 0, currentCol = 0;
	for (char ch : userInput) {
		if (ch == '\n') {
			currentLine++;
			currentCol = 0;
		} else if (ch == '\t') {
			currentCol += 4; // tab display width: arrow plus three spaces
		} else {
			currentCol++;
		}
	}

	if (currentLine > 0) {
		cout << moveCursorDown(currentLine);
	}

	if (currentCol > 0) {
		cout << moveCursorRight(currentCol);
	}

	cout.flush();
}

// Helper class to ensure terminal settings are restored on exit.
struct TerminalSettings {
	int fd;
	struct termios orig;
	bool restored;
	TerminalSettings(int fd_in) : fd(fd_in), restored(false) { tcgetattr(fd, &orig); }
	~TerminalSettings() { restore(); }
	void restore() {
		if (!restored) {
			tcsetattr(fd, TCSAFLUSH, &orig);
			restored = true;
		}
	}
};

string wrapText(const string& text, int wrapWidth) {
	if (wrapWidth <= 0) {
		return text;
	}

	string wrapped;
	size_t start = 0;
	while (start < text.size()) {
		size_t newlinePos = text.find('\n', start);
		string paragraph;
		if (newlinePos == string::npos) {
			paragraph = text.substr(start);
			start = text.size();
		} else {
			paragraph = text.substr(start, newlinePos - start);
			start = newlinePos + 1;
		}

		istringstream iss(paragraph);
		vector<string> words{istream_iterator<string>(iss), istream_iterator<string>()};
		string line;
		for (const auto& word : words) {
			if (line.empty()) {
				line = word;
			} else if (line.size() + 1 + word.size() <= static_cast<size_t>(wrapWidth)) {
				line += " " + word;
			} else {
				wrapped += line + "\n";
				line = word;
			}
		}

		if (!line.empty()) {
			wrapped += line;
		}

		if (newlinePos != string::npos) {
			wrapped += "\n";
		}
	}

	return wrapped;
}

set<string> computeMisspelledWords(const string& target, const string& userInput) {
	set<string> misspelled;
	size_t i = 0;
	while (i < target.size()) {
		while (i < target.size() && isspace((unsigned char)target[i])) {
			i++;
		}

		size_t start = i;
		while (i < target.size() && !isspace((unsigned char)target[i])) {
			i++;
		}

		if (start < i) {
			bool wordError = false;
			for (size_t j = start; j < i; j++) {
				if (j >= userInput.size() || userInput[j] != target[j]) {
					wordError = true;
					break;
				}
			}
			if (wordError) {
				misspelled.insert(target.substr(start, i - start));
			}
		}
	}

	return misspelled;
}

int main(int argc, char** argv) {
	// Parse command line options
	int wrapWidth = 0;
	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		if (arg == "-w" && i + 1 < argc) {
			try {
				wrapWidth = stoi(argv[++i]);
			} catch (...) {
				cerr << "Invalid wrap width provided" << endl;
				return 1;
			}
		}
	}

	// Get test text from user (multi-line)
	string target = string((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
	if (wrapWidth > 0) {
		target = wrapText(target, wrapWidth);
	}

	// Trim target text to remove leading/trailing whitespace.
	target.erase(target.begin(), find_if(target.begin(), target.end(), [](unsigned char ch) { return !isspace(ch); }));
	target.erase(find_if(target.rbegin(), target.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), target.end());

	// Determine the interactive input file descriptor.
	int input_fd;
	if (isatty(STDIN_FILENO)) {
		input_fd = STDIN_FILENO;
	} else {
		input_fd = open("/dev/tty", O_RDONLY);
		if (input_fd < 0) {
			cerr << "Error: cannot open /dev/tty" << endl;
			return 1;
		}
	}

	// Split target into lines for display.
	vector<string> targetLines;
	{
		size_t start = 0;
		size_t found;
		while ((found = target.find('\n', start)) != string::npos) {
			targetLines.push_back(target.substr(start, found - start));
			start = found + 1;
		}

		targetLines.push_back(target.substr(start));
	}

	// Enable raw mode (non-canonical, no echo) for character-by-character input on input_fd
	TerminalSettings term(input_fd);
	struct termios raw = term.orig;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(input_fd, TCSAFLUSH, &raw);

	string userInput;
	drawState(targetLines, userInput);
	bool timing_started = false;
	chrono::steady_clock::time_point start_time, end_time;
	char c;

	// Move cursor up to the beginning of the printed block and save as restore point.
	cout << moveCursorUp(targetLines.size() - 1);
	cout << ANSI_MOVE_CURSOR_TO_BEGINNING_OF_LINE;
	cout << ANSI_SAVE_CURSOR;
	cout.flush();

	while (true) {
		ssize_t n = read(input_fd, &c, 1);
		if (n <= 0) {
			continue;
		}

		if (!timing_started) {
			start_time = chrono::steady_clock::now();
			timing_started = true;
		}

		// Process input: handle backspace or append character.
		if ((c == 127 || c == '\b') && !userInput.empty()) {
			userInput.pop_back();
		} else if (target[userInput.size()] == '\n' && isspace(c)) {
			userInput.push_back('\n');

			// Determine current line index by counting newline characters.
			int currentLine = 0;
			for (char ch : userInput) {
				if (ch == '\n') {
					currentLine++;
				}
			}

			// If there is a subsequent line in targetLines, inject its leading whitespace.
			if ((size_t)currentLine < targetLines.size()) {
				string prefix;
				for (char ch : targetLines[currentLine]) {
					if (ch == ' ' || ch == '\t') {
						prefix.push_back(ch);
					} else {
						break;
					}
				}

				userInput += prefix;
			}
		} else {
			userInput.push_back(c);
		}

		cout << ANSI_RESTORE_CURSOR;
		size_t totalExpected = drawState(targetLines, userInput);

		cout << ANSI_RESTORE_CURSOR;
		moveCursor(userInput);

		// Check if typing is complete.
		if (userInput.size() >= totalExpected) {
			break;
		}
	}

	end_time = chrono::steady_clock::now();
	term.restore(); // Restore the original terminal settings
	double seconds = chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count();
	double minutes = seconds / 60.0;
	double wpm = (target.size() / 5.0) / minutes;

	set<string> misspelled = computeMisspelledWords(target, userInput);

	cout << "\n\n";

	int minutesInt = seconds / 60;
	int secInt = static_cast<int>(seconds) % 60;
	cout << "Time: " << minutesInt << ":" << (secInt < 10 ? "0" : "") << secInt << endl;
	cout << "WPM: " << wpm << endl;

	if (misspelled.empty()) {
		cout << "No mistakes! 🎉" << endl;
	} else {
		cout << "Misspelled words: ";
		bool first = true;
		for (const auto& word : misspelled) {
			if (!first) {
				cout << ", ";
			}
			cout << word;
			first = false;
		}
		cout << endl;
	}

	// Close input_fd if we opened /dev/tty
	if (input_fd != STDIN_FILENO) {
		close(input_fd);
	}

	return 0;
}
