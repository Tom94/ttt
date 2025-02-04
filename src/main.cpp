// This file was developed by Thomas Müller <contact@tom94.net>.
// It is published under the GPLv3 License; see the LICENSE file.

#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

const string ANSI_SAVE_CURSOR = "\033[s";
const string ANSI_RESTORE_CURSOR = "\033[u";
const string ANSI_GRAY = "\033[90m";
const string ANSI_RESET = "\033[0m";
const string ANSI_CORRECT = "\033[97m";
const string ANSI_INCORRECT = "\033[91m";
const string ANSI_CLEAR_LINE = "\r\033[2K";

string displayChar(char c, bool leading = false) {
	if (leading && c == ' ') {
		return "·";
	}

	if (leading && c == '\t') {
		return "→   ";
	}

	return string(1, c);
}

string moveCursorDown(int n) { return "\033[" + to_string(n) + "B"; }

string moveCursorRight(int n) { return "\033[" + to_string(n) + "C"; }

size_t drawTestState(const vector<string>& targetLines, const string& userInput) {
	// Restore the cursor to the saved position.
	cout << ANSI_RESTORE_CURSOR;

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
					cout << ANSI_INCORRECT << displayChar(userInput[j], isLeading) << ANSI_RESET;
				}
			} else {
				cout << ANSI_GRAY << displayChar(targetLines[i][local_j], isLeading) << ANSI_RESET;
			}
		}

		if (i < targetLines.size() - 1) {
			cout << "\n";
		}
	}

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

	// Reposition the cursor relative to the previously saved position.
	cout << ANSI_RESTORE_CURSOR;
	if (currentLine > 0) {
		cout << moveCursorDown(currentLine);
	}

	if (currentCol > 0) {
		cout << moveCursorRight(currentCol);
	}

	cout.flush();

	size_t totalExpected = offsets.back() + targetLines.back().size();
	return totalExpected;
}

// Helper class to ensure terminal settings are restored on exit.

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

	// If wrap option provided, perform word wrapping.
	if (wrapWidth > 0) {
		string wrapped;
		size_t start = 0;
		while (start < target.size()) {
			size_t newlinePos = target.find('\n', start);
			string paragraph;
			if (newlinePos == string::npos) {
				paragraph = target.substr(start);
				start = target.size();
			} else {
				paragraph = target.substr(start, newlinePos - start);
				start = newlinePos + 1;
			}

			istringstream iss(paragraph);
			vector<string> words;
			string word;
			while (iss >> word) {
				words.push_back(word);
			}

			string line;
			for (size_t i = 0; i < words.size(); i++) {
				if (line.empty()) {
					line = words[i];
				} else if (line.size() + words[i].size() < static_cast<size_t>(wrapWidth)) {
					line += " " + words[i];
				} else {
					wrapped += line + "\n";
					line = words[i];
				}
			}

			if (!line.empty()) {
				wrapped += line;
			}

			// Preserve the original newline.
			if (newlinePos != string::npos) {
				wrapped += "\n";
			}
		}

		target = wrapped;
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

	// Move cursor up to the beginning of the printed block.
	cout << ANSI_SAVE_CURSOR;
	cout.flush();

	// Enable raw mode (non-canonical, no echo) for character-by-character input on input_fd
	TerminalSettings term(input_fd);
	struct termios raw = term.orig;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(input_fd, TCSAFLUSH, &raw);

	string userInput;
	drawTestState(targetLines, userInput);
	bool timing_started = false;
	chrono::steady_clock::time_point start_time, end_time;
	char c;

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

		size_t totalExpected = drawTestState(targetLines, userInput);

		// Check if typing is complete.
		if (userInput.size() >= totalExpected) {
			break;
		}
	}
	end_time = chrono::steady_clock::now();
	term.restore(); // Restore the original terminal settings

	end_time = chrono::steady_clock::now();
	double seconds = chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count();
	double minutes = seconds / 60.0;
	double wpm = (target.size() / 5.0) / minutes;

	// Calculate error count based on final user input compared to target.
	int errors = 0;
	for (size_t i = 0; i < target.size(); i++) {
		if (userInput[i] != target[i]) {
			errors++;
		}
	}

	cout << "\n\n";
	cout << "Time: " << seconds << " seconds" << endl;
	cout << "WPM: " << wpm << endl;
	cout << "Errors: " << errors << endl;

	// Close input_fd if we opened /dev/tty
	if (input_fd != STDIN_FILENO) {
		close(input_fd);
	}

	return 0;
}
