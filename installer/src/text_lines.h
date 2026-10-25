// Every text file the installer reads is split into lines here: CRLF, a lone CR and LF all end a
// line (Python's universal newlines, as install.py reads), the ends not kept, a last line without
// its end kept. The installer writes its files with CRLF on Windows, and three bugs came of
// readers that did not expect it (docs/regressions.md, "The mod's texts read with CRLF"); the
// proxy's x360_read_text keeps the same rule.
#pragma once
#include <vector>

namespace x360ui {

template <class S>
std::vector<S> text_lines(const S& t) {
  std::vector<S> lines;
  S line;
  for (size_t i = 0; i < t.size(); i++) {
    if (t[i] == '\r' || t[i] == '\n') {
      if (t[i] == '\r' && i + 1 < t.size() && t[i + 1] == '\n') i++;
      lines.push_back(line);
      line.clear();
    } else {
      line += t[i];
    }
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace x360ui
