// This file is part of UniLib <http://github.com/ufal/unilib/>.
//
// Copyright 2014 Institute of Formal and Applied Linguistics, Faculty of
// Mathematics and Physics, Charles University in Prague, Czech Republic.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

// Simple testing framework
int passed = 0, failed = 0;

template <class T>
void test_dump(T data) {
  cerr << data;
}

template <>
void test_dump(char32_t data) {
  cerr << hex << data << oct;
}

template <class T>
void test_dump(basic_string<T> data) {
  cerr << hex;
  for (auto&& chr : data) cerr << unsigned(chr) << ' ';
  cerr << oct;
}

template<class Test, class I, class O>
void test(Test test, I input, O output) {
  O result = test(input);
  if (result != output) {
    cerr << "Failed, expected ";
    test_dump(output);
    cerr << ", but got ";
    test_dump(result);
    cerr << "." << endl;
    failed++;
  } else
    passed++;
}

int test_summary() {
  cerr << "Passed: " << passed << ", failed: " << failed << endl;
  return failed != 0;
}

void split(const string& text, char sep, vector<string>& tokens) {
  tokens.clear();
  if (text.empty()) return;

  string::size_type index = 0;
  for (string::size_type next; (next = text.find(sep, index)) != string::npos; index = next + 1)
    tokens.emplace_back(text, index, next - index);
  tokens.emplace_back(text, index);
}
