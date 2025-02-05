// This file was developed by Thomas Müller <contact@tom94.net>.
// It is published under the GPLv3 License; see the LICENSE file.

#include <cctype>
#include <chrono>
#include <fcntl.h>
#include <format>
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

string move_cursor_up(int n) { return std::format("\033[{}A", n); }
string move_cursor_down(int n) { return std::format("\033[{}B", n); }
string move_cursor_right(int n) { return std::format("\033[{}C", n); }
string move_cursor_left(int n) { return std::format("\033[{}D", n); }

string display_char(char c, bool leading = false) {
	if (leading && c == '\t') {
		return "    ";
	}

	return string(1, c);
}

size_t draw_state(const vector<string>& target_lines, const string& user_input) {
	// Compute cumulative offsets for each line within the displayed block.
	vector<size_t> offsets(target_lines.size());
	size_t cum = 0;
	for (size_t i = 0; i < target_lines.size(); i++) {
		offsets[i] = cum;
		cum += target_lines[i].size() + 1; // +1 for the newline
	}

	// Update each displayed line without clearing the entire screen.
	for (size_t i = 0; i < target_lines.size(); i++) {
		// Compute leading whitespace count for the current line.
		int indent = 0;
		while (indent < (int)target_lines[i].size() && (target_lines[i][indent] == ' ' || target_lines[i][indent] == '\t')) {
			indent++;
		}

		cout << ANSI_CLEAR_LINE;
		int lineStart = offsets[i];
		int line_end = offsets[i] + target_lines[i].size();
		for (int j = lineStart; j < line_end; j++) {
			int local_j = j - offsets[i];
			bool is_leading = (local_j < indent);
			if (j < (int)user_input.size()) {
				if (user_input[j] == target_lines[i][local_j]) {
					cout << ANSI_CORRECT << display_char(target_lines[i][local_j], is_leading) << ANSI_RESET;
				} else {
					if (isspace(user_input[j])) {
						cout << ANSI_INCORRECT_WHITESPACE << display_char(user_input[j], is_leading) << ANSI_RESET;
					} else {
						cout << ANSI_INCORRECT << display_char(user_input[j], is_leading) << ANSI_RESET;
					}
				}
			} else {
				cout << ANSI_GRAY << display_char(target_lines[i][local_j], is_leading) << ANSI_RESET;
			}
		}

		if (i < target_lines.size() - 1) {
			cout << "\n";
		}
	}

	cout.flush();
	size_t total_expected = offsets.back() + target_lines.back().size();
	return total_expected;
}

void move_cursor(const string& user_input) {
	// Determine the cursor's current row and column based on userInput (accounting for tab width).
	int current_line = 0, current_col = 0;
	for (char ch : user_input) {
		if (ch == '\n') {
			++current_line;
			current_col = 0;
		} else if (ch == '\t') {
			current_col += 4; // tab display width: arrow plus three spaces
		} else {
			++current_col;
		}
	}

	if (current_line > 0) {
		cout << move_cursor_down(current_line);
	}

	if (current_col > 0) {
		cout << move_cursor_right(current_col);
	}

	cout.flush();
}

// Helper class to ensure terminal settings are restored on exit.
struct TerminalSettings {
	int fd;
	struct termios orig;
	bool restored;
	TerminalSettings(int fd_in) : fd{fd_in}, restored{false} { tcgetattr(fd, &orig); }
	~TerminalSettings() { restore(); }
	void restore() {
		if (!restored) {
			tcsetattr(fd, TCSAFLUSH, &orig);
			restored = true;
		}
	}
};

string wrap_text(const string& text, int wrap_width) {
	if (wrap_width <= 0) {
		return text;
	}

	string wrapped;
	size_t start = 0;
	while (start < text.size()) {
		size_t newline_pos = text.find('\n', start);
		string paragraph;
		if (newline_pos == string::npos) {
			paragraph = text.substr(start);
			start = text.size();
		} else {
			paragraph = text.substr(start, newline_pos - start);
			start = newline_pos + 1;
		}

		istringstream iss(paragraph);
		vector<string> words{istream_iterator<string>(iss), istream_iterator<string>()};
		string line;
		for (const auto& word : words) {
			if (line.empty()) {
				line = word;
			} else if (line.size() + 1 + word.size() <= static_cast<size_t>(wrap_width)) {
				line += " " + word;
			} else {
				wrapped += line + "\n";
				line = word;
			}
		}

		if (!line.empty()) {
			wrapped += line;
		}

		if (newline_pos != string::npos) {
			wrapped += "\n";
		}
	}

	return wrapped;
}

set<string> find_misspelled_words(const string& target, const string& user_input) {
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
				if (j >= user_input.size() || user_input[j] != target[j]) {
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
	int wrap_width{0};
	for (int i = 1; i < argc; i++) {
		string arg{argv[i]};
		if (arg == "-w" && i + 1 < argc) {
			try {
				wrap_width = stoi(argv[++i]);
			} catch (...) {
				cerr << "Invalid wrap width provided" << endl;
				return 1;
			}
		}
	}

	// Get test text from user (multi-line)
	string target{istreambuf_iterator<char>(cin), istreambuf_iterator<char>()};
	if (wrap_width > 0) {
		target = wrap_text(target, wrap_width);
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
	vector<string> target_lines;
	{
		size_t start = 0;
		size_t found;
		while ((found = target.find('\n', start)) != string::npos) {
			target_lines.push_back(target.substr(start, found - start));
			start = found + 1;
		}

		target_lines.push_back(target.substr(start));
	}

	// Enable raw mode (non-canonical, no echo) for character-by-character input on input_fd
	TerminalSettings term(input_fd);
	struct termios raw = term.orig;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(input_fd, TCSAFLUSH, &raw);

	string user_input;
	draw_state(target_lines, user_input);
	bool timing_started = false;
	chrono::steady_clock::time_point start_time, end_time;
	char c;

	// Move cursor up to the beginning of the printed block and save as restore point.
	cout << move_cursor_up(target_lines.size() - 1);
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

		// Process input: handle backspace, Ctrl-W (delete last word), or append character.
		if ((c == 127 || c == '\b') && !user_input.empty()) {
			user_input.pop_back();
		} else if (c == 23 && !user_input.empty()) {
			// Delete any trailing whitespace first.
			while (!user_input.empty() && isspace(user_input.back())) {
				user_input.pop_back();
			}
			// Then delete characters until the previous whitespace.
			while (!user_input.empty() && !isspace(user_input.back())) {
				user_input.pop_back();
			}
		} else if (target[user_input.size()] == '\n' && isspace(c)) {
			user_input.push_back('\n');

			// Determine current line index by counting newline characters.
			int current_line = 0;
			for (char ch : user_input) {
				if (ch == '\n') {
					++current_line;
				}
			}

			// If there is a subsequent line in target_lines, inject its leading whitespace.
			if ((size_t)current_line < target_lines.size()) {
				string prefix;
				for (char ch : target_lines[current_line]) {
					if (ch == ' ' || ch == '\t') {
						prefix.push_back(ch);
					} else {
						break;
					}
				}

				user_input += prefix;
			}
		} else {
			user_input.push_back(c);
		}

		cout << ANSI_RESTORE_CURSOR;
		size_t total_expected = draw_state(target_lines, user_input);

		cout << ANSI_RESTORE_CURSOR;
		move_cursor(user_input);

		// Check if typing is complete.
		if (user_input.size() >= total_expected) {
			break;
		}
	}

	end_time = chrono::steady_clock::now();
	term.restore(); // Restore the original terminal settings
	double seconds = chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count();
	double minutes = seconds / 60.0;
	double wpm = (target.size() / 5.0) / minutes;

	// Compute the number of correct characters.
	size_t correct_chars = 0;
	for (size_t i = 0; i < target.size(); i++) {
		if (target[i] == user_input[i]) {
			++correct_chars;
		}
	}

	double accuracy = (static_cast<double>(correct_chars) / target.size()) * 100.0;
	double cpm = target.size() / minutes;

	set<string> misspelled = find_misspelled_words(target, user_input);

	int minutes_int = seconds / 60;
	int sec_int = static_cast<int>(seconds) % 60;

	cout << std::format("\n\nTime: {}:{:02}\nWPM: {}\nCPM: {}\nAccuracy: {:.2f}%\n", minutes_int, sec_int, wpm, cpm, accuracy);

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
