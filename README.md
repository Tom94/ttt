# terminal typing test

This is a terminal program to test your typing speed, written in C++.

## How to run

Simply pipe the text you'd like to type into `ttt`.
I recommend generating some on the fly with a language model and the amazing [aichat](https://github.com/sigoden/aichat) program.

```bash
$ aichat "Write a short paragraph about space travel." | ttt
```

## How to build

This project uses CMake and has otherwise no dependencies.
To download and build it, run the following commands:

```bash
$ git clone https://github.com/tom94/ttt
$ cmake -B build
$ cmake --build build
```

## How it was made

I wanted to test o3-mini's coding ability and this seemed like a fun way to do it.
Ergo: this project was mostly written by AI, with me stepping in just to fix a bug here and there, but mostly just prompting away.
It's amazing how far AI has come!

I used [neovim](https://neovim.io/) with the [avante plugin](https://github.com/yetone/avante.nvim), but if you're not already in the vim ecosystem, you can do the same in [cursor](https://www.cursor.com/).

## TODO

- [ ] Better (configurable?) colors
- [ ] Windows support
- [ ] Squash bugs as they appear

## License

GPLv3, because why not.
