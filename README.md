# terminal typing test

**ttt** is a terminal program to test your typing speed, written in C++.
Supports code (auto indentation) and unicode (Asian languages).

## How to run

Simply pipe the text you'd like to type into `ttt`.

https://github.com/user-attachments/assets/2ba5599f-d799-4771-878d-0413380693fa

In practice, I recommend generating text on the fly with a language model, e.g., via [aichat](https://github.com/sigoden/aichat).

```bash
$ aichat "Short poetic paragraph about space travel." | ttt -w 80
```
```
Red dust fell behind as the ship cut through Mars' orbit. Stars beckoned. The
void stretched endless and dark. We sailed on.

Time: 0:10, WPM: 143, Accuracy: 100.00% 🎉
```

### Options

- `-w`, `--wrap` to word wrap to the given width (no wrapping by default)
- `-h`, `--help` to show help info
- `-v`, `--version` to show version info

## How to build

This project uses CMake and has otherwise no dependencies.
To download and build it, run the following commands:

```bash
$ git clone https://github.com/tom94/ttt
$ cmake -B build
$ cmake --build build
```

## How it was made

I wanted to play around with AI-assisted coding and creating **ttt** seemed like a fun way to do it.
Ergo: this project was mostly written by AI, with me stepping in just to fix a bug here and there, but mostly just prompting away.
It's amazing how far AI has come!

I used [neovim](https://neovim.io/) with the [avante plugin](https://github.com/yetone/avante.nvim), but if you're not already in the vim ecosystem, you can do the same in [cursor](https://www.cursor.com/).

## TODO

- [ ] Better (configurable?) colors
- [ ] Windows support
- [ ] Squash bugs as they appear

## Shoutouts

- [aichat](https://github.com/sigoden/aichat): the perfect companion to **ttt**
- [unilib](https://github.com/ufal/unilib): for making unicode handling a breeze
- [tt](https://github.com/lemnos/tt): another terminal typing test with different feature set. differences to **ttt**:
  - doesn't support code or unicode
  - has a full-screen tui rather than being inline
  - more CLI options and features, including built-in word lists


## License

GPLv3, because why not.
