// This file was developed by Thomas Müller <contact@tom94.net>.
// It is published under the GPLv3 License; see the LICENSE file.

#include <cmrc/cmrc.hpp>

#include <json/json.hpp>

#include <unilib/uninorms.h>
#include <unilib/utf.h>

#include <wcwidth/wcwidth.h>

#include <cctype>
#include <chrono>
#include <cstring>
#include <format>
#include <iostream>
#include <iterator>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <wchar.h>

#ifdef _WIN32
#	define NOMINMAX
#	include <io.h>
#	include <windows.h>
#	undef NOMINMAX
#else
#	include <fcntl.h>
#	include <sys/ioctl.h>
#	include <termios.h>
#	include <unistd.h>
#endif

using namespace std;
using namespace nlohmann;

CMRC_DECLARE(ttt);

namespace ttt {

auto g_fs = cmrc::ttt::get_filesystem();

size_t g_tab_width = 4;

template <typename T> class ScopeGuard {
public:
	ScopeGuard(const T& callback) : mCallback{callback} {}
	ScopeGuard(T&& callback) : mCallback{std::move(callback)} {}
	ScopeGuard(const ScopeGuard<T>& other) = delete;
	ScopeGuard& operator=(const ScopeGuard<T>& other) = delete;
	ScopeGuard(ScopeGuard<T>&& other) { *this = std::move(other); }
	ScopeGuard& operator=(ScopeGuard<T>&& other) {
		mCallback = std::move(other.mCallback);
		other.mCallback = {};
	}
	~ScopeGuard() { mCallback(); }

private:
	T mCallback;
};

template <typename T> std::string join(const T& components, const std::string& delim) {
	std::ostringstream s;
	for (const auto& component : components) {
		if (&components[0] != &component) {
			s << delim;
		}
		s << component;
	}

	return s.str();
}

vector<string> split(string text, const string& delim) {
	vector<string> result;
	size_t begin = 0;
	while (true) {
		size_t end = text.find_first_of(delim, begin);
		if (end == string::npos) {
			result.emplace_back(text.substr(begin));
			return result;
		} else {
			result.emplace_back(text.substr(begin, end - begin));
			begin = end + 1;
		}
	}

	return result;
}

// Returns the number of bytes in a UTF-8 character based on its first byte
int utf8_char_length(unsigned char first_byte) {
	if ((first_byte & 0x80) == 0) {
		return 1;
	}

	if ((first_byte & 0xE0) == 0xC0) {
		return 2;
	}

	if ((first_byte & 0xF0) == 0xE0) {
		return 3;
	}

	if ((first_byte & 0xF8) == 0xF0) {
		return 4;
	}

	return 1; // Invalid UTF-8 byte, treat as single byte
}

// Check if this byte is a continuation byte in UTF-8
bool is_utf8_continuation(unsigned char byte) { return (byte & 0xC0) == 0x80; }

// Check if this byte starts a combining character
bool is_combining_char(const string& str, size_t pos) {
	if (pos >= str.length()) {
		return false;
	}

	wchar_t wc;
	char buf[8] = {0};
	int len = utf8_char_length(str[pos]);
	strncpy(buf, &str[pos], len);
	mbtowc(&wc, buf, len);

	// Unicode ranges for combining characters
	return (wc >= 0x0300 && wc <= 0x036F) || // Combining Diacritical Marks
		(wc >= 0x1AB0 && wc <= 0x1AFF) ||    // Combining Diacritical Marks Extended
		(wc >= 0x1DC0 && wc <= 0x1DFF) ||    // Combining Diacritical Marks Supplement
		(wc >= 0x20D0 && wc <= 0x20FF) ||    // Combining Diacritical Marks for Symbols
		(wc >= 0xFE20 && wc <= 0xFE2F);      // Combining Half Marks
}

// Find the end of the current grapheme cluster
size_t find_grapheme_cluster_end(const string& str, size_t start) {
	if (start >= str.length()) {
		return start;
	}

	size_t pos = start;
	pos += utf8_char_length(str[pos]);

	// Include any following combining characters
	while (pos < str.length() && is_combining_char(str, pos)) {
		pos += utf8_char_length(str[pos]);
	}

	// Check for emoji sequences
	if (pos < str.length()) {
		using byte = unsigned char;
		byte next = str[pos];
		// Check for emoji modifiers, ZWJ sequences, etc.
		if ((next == 0xF0 && pos + 3 < str.length()) ||                                                             // Emoji
			(next == 0xE2 && pos + 2 < str.length() && (byte)str[pos + 1] == 0x80 && (byte)str[pos + 2] == 0x8D)) { // ZWJ
			return find_grapheme_cluster_end(str, pos);
		}
	}

	return pos;
}

// Get the display width of a UTF-8 character
int get_char_width(const string& str, size_t pos) {
	if (pos >= str.length()) {
		return 0;
	}

	if (is_utf8_continuation(str[pos])) {
		return 1;
	}

	// Tab width
	if (str[pos] == '\t') {
		return g_tab_width;
	}

	// Convert the UTF-8 character to a wide character
	wchar_t wc;
	char buf[8] = {0};
	int len = utf8_char_length(str[pos]);
	strncpy(buf, &str[pos], len);
	mbtowc(&wc, buf, len);

	if (wc >= 0x1F300) {
		// Unicode range for emojis and other symbols
		return 2;
	}

	int width = mk_wcwidth(wc);
	return width >= 0 ? width : 1;
}

// Get the next UTF-8 character position
size_t next_char_pos(const string& str, size_t pos) {
	if (pos >= str.length()) {
		return str.length();
	}

	return pos + utf8_char_length(str[pos]);
}

// Get the previous UTF-8 character position
size_t prev_char_pos(const string& str, size_t pos) {
	if (pos <= 0) {
		return 0;
	}

	size_t prev = pos - 1;
	while (prev > 0 && (str[prev] & 0xC0) == 0x80) {
		prev--;
	}

	return prev;
}

const string ANSI_SAVE_CURSOR = "\033[s";
const string ANSI_RESTORE_CURSOR = "\033[u";
const string ANSI_GRAY = "\033[38;5;243m";
const string ANSI_RESET = "\033[0m";
const string ANSI_CORRECT = "\033[0m";
const string ANSI_INCORRECT = "\033[38;5;9m";
const string ANSI_INCORRECT_WHITESPACE = "\033[41m";
const string ANSI_CLEAR_LINE = "\r\033[2K";
const string ANSI_MOVE_CURSOR_TO_BEGINNING_OF_LINE = "\r\033[G";

string move_cursor_up(int n) { return std::format("\033[{}A", n); }
string move_cursor_down(int n) { return std::format("\033[{}B", n); }
string move_cursor_right(int n) { return std::format("\033[{}C", n); }
string move_cursor_left(int n) { return std::format("\033[{}D", n); }

string display_char(const string& str, size_t pos, bool leading = false) {
	if (pos >= str.length()) {
		return "";
	}

	if (leading && str[pos] == '\t') {
		return "    ";
	}

	// Skip if we're in the middle of a UTF-8 character
	if (is_utf8_continuation(str[pos])) {
		return "";
	}

	int len = utf8_char_length(str[pos]);
	return str.substr(pos, len);
}

string nfd(const string& str) {
	u32string decoded;
	unilib::utf::decode(str.c_str(), decoded);
	unilib::uninorms::nfd(decoded);
	string result;
	unilib::utf::encode(decoded, result);
	return result;
}

size_t draw_state(const vector<string>& target_lines, string user_input) {
	user_input = nfd(user_input);

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
		int line_start = offsets[i];
		int line_end = offsets[i] + target_lines[i].size();
		for (int j = line_start; j < line_end;) {
			int local_j = j - offsets[i];
			bool is_leading = (local_j < indent);
			if (j < (int)user_input.size()) {
				string target_char = display_char(target_lines[i], local_j, is_leading);
				string user_char = display_char(user_input, j, is_leading);

				if (!target_char.empty()) { // Only process if it's a valid character start
					if (user_char == target_char) {
						cout << ANSI_CORRECT << target_char << ANSI_RESET;
					} else {
						if (isspace(user_input[j])) {
							cout << ANSI_INCORRECT_WHITESPACE << user_char << ANSI_RESET;
						} else {
							cout << ANSI_INCORRECT << user_char << ANSI_RESET;
						}
					}
				}
			} else if (!is_utf8_continuation(target_lines[i][local_j])) {
				cout << ANSI_GRAY << display_char(target_lines[i], local_j, is_leading) << ANSI_RESET;
			}

			// Move to the next character
			if (is_utf8_continuation(target_lines[i][local_j])) {
				j++; // Skip continuation bytes
			} else {
				j += utf8_char_length(target_lines[i][local_j]);
			}
		}

		if (i < target_lines.size() - 1) {
			cout << "\n";
		}
	}

	size_t total_expected = offsets.back() + target_lines.back().size();
	return total_expected;
}

void move_cursor(const string& user_input) {
	// Determine the cursor's current row and column based on userInput (accounting for tab width).
	int current_line = 0, current_col = 0;
	size_t pos = 0;
	while (pos < user_input.length()) {
		if (user_input[pos] == '\n') {
			++current_line;
			current_col = 0;
			pos++;
		} else {
			current_col += get_char_width(user_input, pos);
			pos = next_char_pos(user_input, pos);
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
#ifdef _WIN32
struct TerminalSettings {
	HANDLE hStdin;
	DWORD orig_mode;
	bool restored;
	TerminalSettings(int) : restored{false} {
		hStdin = GetStdHandle(STD_INPUT_HANDLE);
		GetConsoleMode(hStdin, &orig_mode);

		// Enable raw input mode
		DWORD mode = orig_mode;
		mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
		SetConsoleMode(hStdin, mode);
	}
	~TerminalSettings() { restore(); }
	void restore() {
		if (!restored) {
			SetConsoleMode(hStdin, orig_mode);
			restored = true;
		}
	}
};
#else
struct TerminalSettings {
	int fd;
	struct termios orig;
	bool restored;
	TerminalSettings(int fd_in) : fd{fd_in}, restored{false} {
		tcgetattr(fd, &orig);

		// Enable raw input mode
		struct termios raw = orig;
		raw.c_lflag &= ~(ECHO | ICANON);
		tcsetattr(fd, TCSAFLUSH, &raw);
	}
	~TerminalSettings() { restore(); }
	void restore() {
		if (!restored) {
			tcsetattr(fd, TCSAFLUSH, &orig);
			restored = true;
		}
	}
};
#endif

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
			// If the word is longer than wrap_width, break it up
			if (word.size() > static_cast<size_t>(wrap_width)) {
				// First, add any existing line content
				if (!line.empty()) {
					wrapped += line + "\n";
					line.clear();
				}

				// Break up the long word while respecting grapheme clusters
				size_t word_start = 0;
				size_t current_width = 0;
				size_t chunk_start = word_start;

				while (word_start < word.size()) {
					size_t cluster_end = find_grapheme_cluster_end(word, word_start);
					size_t cluster_size = cluster_end - word_start;

					// Get the display width of this cluster
					int cluster_width = 0;
					for (size_t pos = word_start; pos < cluster_end; pos += utf8_char_length(word[pos])) {
						cluster_width += get_char_width(word, pos);
					}

					if (current_width + cluster_width > static_cast<size_t>(wrap_width)) {
						// This cluster would exceed the wrap width
						if (chunk_start < word_start) {
							// Output the accumulated chunk
							if (chunk_start > 0) {
								wrapped += "\n";
							}
							wrapped += word.substr(chunk_start, word_start - chunk_start);
							chunk_start = word_start;
							current_width = 0;
						} else if (cluster_width > wrap_width) {
							// Single cluster wider than wrap width - force break
							if (word_start > 0) {
								wrapped += "\n";
							}
							wrapped += word.substr(word_start, cluster_size);
							word_start = cluster_end;
							chunk_start = word_start;
							current_width = 0;
							continue;
						}
					}

					current_width += cluster_width;
					word_start = cluster_end;
				}

				// Output any remaining chunk
				if (chunk_start < word_start) {
					if (chunk_start > 0) {
						wrapped += "\n";
					}
					wrapped += word.substr(chunk_start, word_start - chunk_start);
				}

				continue;
			}

			// Normal word handling
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

void print_help() {
	cout << "Usage: ttt [OPTIONS]\n"
		 << "A terminal-based typing test.\n"
		 << "\n"
		 << "Options:\n"
		 << "  -h, --help                 Show this help message and exit\n"
		 << "  -v, --version              Show version information and exit\n"
		 << "  -n, --nwords N [LISTNAME]  N random words [word list name]\n"
		 << "  -q, --quote [LISTNAME]     Random quote from list [quote list name]\n"
		 << "  -t, --tab WIDTH            Tab width\n"
		 << "  -w, --wrap WIDTH           Word-wrap text at WIDTH characters\n"
		 << "\n"
		 << "Input text via stdin. Press ESC or Ctrl-C to quit.\n";
	cout.flush();
}

void print_version() { cout << "ttt — terminal typing test" << endl << "version " << TTT_VERSION << endl; }

string ls(const string& path) {
	ostringstream result;
	bool first = true;
	for (const auto& entry : g_fs.iterate_directory(path)) {
		if (!first) {
			result << ", ";
		}

		first = false;
		result << entry.filename();
	}

	return result.str();
}

size_t console_width() {
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
		return csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
#else
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
		return w.ws_col;
	}
#endif

	return 0;
}

vector<string> get_word_list(const string& name) {
	vector<string> words;

	try {
		auto words_file = g_fs.open(format("resources/words/{}", name));
		string words_string = {words_file.cbegin(), words_file.cend()};
		return split(words_string, "\n");
	} catch (...) { throw invalid_argument{format("Invalid word list name provided. Available lists: {}", ls("resources/words"))}; }
}

json get_quote_list(const string& name) {
	try {
		auto quotes_file = g_fs.open(format("resources/quotes/{}", name));
		return json::parse(quotes_file);
	} catch (...) { throw invalid_argument{format("Invalid quote list name provided. Available lists: {}", ls("resources/quotes"))}; }
}

random_device g_rd;
mt19937 g_rd_gen(g_rd());

int main(const vector<string>& args) {
	// Parse command line options
	string quote_list_name = "";
	string word_list_name = "";
	size_t n_words = 20;
	size_t wrap_width = 0;
	for (size_t i = 1; i < args.size(); i++) {
		const string& arg = args[i];
		if (arg == "-h" || arg == "--help") {
			print_help();
			return 0;
		} else if (arg == "-v" || arg == "--version") {
			print_version();
			return 0;
		} else if ((arg == "-n" || arg == "--nwords")) {
			if (i + 1 < args.size()) {
				try {
					n_words = stoul(args[++i]);
				} catch (...) { throw invalid_argument{"Invalid number of words provided"}; }
			}

			if (i + 1 < args.size() && !args[i + 1].starts_with('-')) {
				word_list_name = args[++i];
			} else {
				word_list_name = "1000en";
			}
		} else if ((arg == "-q" || arg == "--quote")) {
			if (i + 1 < args.size() && !args[i + 1].starts_with('-')) {
				quote_list_name = args[++i];
			} else {
				quote_list_name = "en";
			}
		} else if ((arg == "-t" || arg == "--tab") && i + 1 < args.size()) {
			try {
				g_tab_width = stoul(args[++i]);
			} catch (...) { throw invalid_argument{"Invalid tab width provided"}; }
		} else if ((arg == "-w" || arg == "--wrap") && i + 1 < args.size()) {
			try {
				wrap_width = stoul(args[++i]);
			} catch (...) { throw invalid_argument{"Invalid wrap width provided"}; }
		}
	}

	// While this program *technically* works without wrapping, it's better to set the wrap width to the terminal width
	// so that words don't get broken up by the terminal which doesn't care for word boundaries.
	if (wrap_width == 0) {
		wrap_width = console_width();
	}

	// Get target either from a word list, a quote list, or stdin
	string target;
	if (!word_list_name.empty()) {
		auto words = get_word_list(word_list_name);
		if (words.size() == 0) {
			throw runtime_error{"No words found"};
		}

		uniform_int_distribution<> dis(0, words.size() - 1);

		vector<string> selected_words;
		for (size_t i = 0; i < n_words; i++) {
			selected_words.push_back(words[dis(g_rd_gen)]);
		}

		target = join(selected_words, " ");
	} else if (!quote_list_name.empty()) {
		json quotes = get_quote_list(quote_list_name);
		if (quotes.size() == 0) {
			throw runtime_error{"No quotes found"};
		}

		uniform_int_distribution<> dis(0, quotes.size() - 1);
		const json& quote = quotes[dis(g_rd_gen)];

		target = quote.value("text", "");

		string attribution = quote.value("attribution", "");
		if (attribution.empty()) {
			attribution = "Unknown person";
		}

		cout << attribution << ": " << endl;
	} else {
		target = string{istreambuf_iterator<char>(cin), istreambuf_iterator<char>()};
	}

	if (target.empty()) {
		throw runtime_error{"No text provided"};
	}

	target = nfd(target);

	if (wrap_width > 0) {
		target = wrap_text(target, wrap_width);
	}

	// Remove trailing whitespace from target text
	target.erase(find_if(target.rbegin(), target.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), target.end());

	// Determine the interactive input file descriptor.
	int input_fd;
#ifdef _WIN32
	input_fd = _fileno(stdin);
	// Enable ANSI escape sequences on Windows
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
#else
	if (isatty(STDIN_FILENO)) {
		input_fd = STDIN_FILENO;
	} else {
		input_fd = open("/dev/tty", O_RDONLY);
		if (input_fd < 0) {
			throw runtime_error{"Cannot open /dev/tty"};
		}
	}

	ScopeGuard guard{[&input_fd] {
		if (input_fd != STDIN_FILENO) {
			close(input_fd);
		}
	}};
#endif

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

	// The terminal settings object enables raw input mode and automatically reverts to default settings when destructed
	TerminalSettings term(input_fd);

	string user_input;
	draw_state(target_lines, user_input);
	bool timing_started = false;
	chrono::steady_clock::time_point start_time, end_time;
	char c;

	// Move cursor up to the beginning of the printed block and save as restore point.
	if (target_lines.size() > 1) {
		cout << move_cursor_up(target_lines.size() - 1);
	}

	cout << ANSI_MOVE_CURSOR_TO_BEGINNING_OF_LINE;
	cout << ANSI_SAVE_CURSOR;
	cout.flush();

	while (true) {
#ifdef _WIN32
		DWORD n;
		if (!ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &n, NULL)) {
			continue;
		}
#else
		ssize_t n = read(input_fd, &c, 1);
#endif

		if (n <= 0) {
			continue;
		}

		if (!timing_started) {
			start_time = chrono::steady_clock::now();
			timing_started = true;
		}

		if (c == 27) { // Close on esc
			term.restore();
			cout << "\n\nCancelled.\n";
			return 0;
		} else if ((c == 127 || c == '\b')) { // Backspace
			if (!user_input.empty()) {
				size_t prev = prev_char_pos(user_input, user_input.length());
				user_input.erase(prev);
			}
		} else if (c == 23) { // Ctrl-W (delete word)
			// Delete any trailing whitespace first.
			while (!user_input.empty() && isspace(user_input.back())) {
				user_input.pop_back();
			}

			// Then delete characters until the previous whitespace.
			while (!user_input.empty() && !isspace(user_input.back())) {
				user_input.pop_back();
			}
		} else if (target[user_input.size()] == '\n' && isspace(c)) { // Let the user press space instead of newline
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
			if (isspace(c)) {
				c = ' ';
			}

			user_input.push_back(c);
		}

		cout << ANSI_RESTORE_CURSOR;
		size_t total_expected = draw_state(target_lines, user_input);

		cout << ANSI_RESTORE_CURSOR;
		move_cursor(user_input);

		// Check if typing is complete.
		if (nfd(user_input).size() >= total_expected) {
			break;
		}
	}

	user_input = nfd(user_input);

	end_time = chrono::steady_clock::now();
	term.restore(); // Restore the original terminal settings
	double seconds = chrono::duration_cast<chrono::duration<double>>(end_time - start_time).count();
	double minutes = seconds / 60.0;
	double wpm = (target.size() / 5.0) / minutes;

	size_t n_correct_chars = 0;
	for (size_t i = 0; i < target.size(); i++) {
		if (target[i] == user_input[i]) {
			++n_correct_chars;
		}
	}

	double accuracy = (static_cast<double>(n_correct_chars) / target.size()) * 100.0;

	set<string> misspelled = find_misspelled_words(target, user_input);

	int minutes_int = seconds / 60;
	int sec_int = static_cast<int>(seconds) % 60;

	cout << std::format(
		"\n\nTime: {}:{:02}, WPM: {:.0f}, Accuracy: {:.2f}% {}\n", minutes_int, sec_int, wpm, accuracy, accuracy == 100 ? "🎉" : ""
	);

	if (!misspelled.empty()) {
		cout << "Misspelled words: ";
		bool first = true;
		for (const auto& word : misspelled) {
			if (!first) {
				cout << ", ";
			}

			cout << std::format("\"{}\"", word);
			first = false;
		}
		cout << endl;
	}

	return 0;
}

} // namespace ttt

#ifdef _WIN32
string utf16_to_utf8(const wstring& utf16) {
	string utf8;
	if (!utf16.empty()) {
		int size = WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), NULL, 0, NULL, NULL);
		utf8.resize(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), &utf8[0], size, NULL, NULL);
	}

	return utf8;
}

int wmain(int argc, wchar_t* argv[]) {
	SetConsoleOutputCP(CP_UTF8);
#else
int main(int argc, char* argv[]) {
#endif
	setlocale(LC_ALL, "en_US.UTF-8");

	try {
		// This accelerates I/O significantly by allowing C++ to perform its own buffering. Furthermore, this prevents a
		// failure to forcefully close the stdin thread in case of a shutdown on certain Linux systems.
		ios::sync_with_stdio(false);

		vector<string> arguments;
		for (int i = 0; i < argc; ++i) {
#ifdef _WIN32
			arguments.emplace_back(utf16_to_utf8(argv[i]));
#else
			string arg = argv[i];
			// OSX sometimes (seemingly sporadically) passes the process serial number via a command line parameter. We
			// would like to ignore this.
			if (arg.find("-psn") != 0) {
				arguments.emplace_back(argv[i]);
			}
#endif
		}

		return ttt::main(arguments);
	} catch (const exception& e) {
		cerr << format("\nError: {}", e.what());
		return 1;
	}
}
