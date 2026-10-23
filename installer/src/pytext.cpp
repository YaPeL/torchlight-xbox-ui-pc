#include "pytext.h"

namespace x360ui {
namespace {

void utf8_out(std::string& out, char32_t c) {
  if (c < 0x80) {
    out += char(c);
  } else if (c < 0x800) {
    out += char(0xC0 | (c >> 6));
    out += char(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += char(0xE0 | (c >> 12));
    out += char(0x80 | ((c >> 6) & 0x3F));
    out += char(0x80 | (c & 0x3F));
  } else {
    out += char(0xF0 | (c >> 18));
    out += char(0x80 | ((c >> 12) & 0x3F));
    out += char(0x80 | ((c >> 6) & 0x3F));
    out += char(0x80 | (c & 0x3F));
  }
}

U32 utf16_decode(const uint8_t* p, size_t n, bool little) {
  if (n % 2) throw Error("UTF-16: truncated data");
  U32 out;
  for (size_t i = 0; i < n; i += 2) {
    char32_t u = little ? char32_t(p[i] | p[i + 1] << 8) : char32_t(p[i] << 8 | p[i + 1]);
    if (u >= 0xD800 && u < 0xDC00) {
      if (i + 3 >= n) throw Error("UTF-16: unexpected end of data");
      char32_t v = little ? char32_t(p[i + 2] | p[i + 3] << 8) : char32_t(p[i + 2] << 8 | p[i + 3]);
      if (v < 0xDC00 || v >= 0xE000) throw Error("UTF-16: illegal UTF-16 surrogate");
      u = 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00);
      i += 2;
    } else if (u >= 0xDC00 && u < 0xE000) {
      throw Error("UTF-16: illegal encoding");
    }
    out += u;
  }
  return out;
}

bool ascii_alnum(char32_t c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
char32_t ascii_lower(char32_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

// Characters that re.IGNORECASE folds onto ASCII letters (the dotted and dotless I, the long s,
// the Kelvin sign): met where a name is compared, the answer is Python's, not ours.
bool folds_to_ascii(char32_t c) { return c == 0x130 || c == 0x131 || c == 0x17F || c == 0x212A; }

// re's \b before a word character at i: the start, or a non-word character before.
bool boundary_before(const U32& t, size_t i) { return i == 0 || !py_isword(t[i - 1]); }

size_t skip_spaces(const U32& t, size_t i) {
  while (i < t.size() && py_isspace(t[i])) i++;
  return i;
}

bool same_nocase(const U32& t, size_t i, const U32& name) {
  if (i + name.size() > t.size()) return false;
  for (size_t k = 0; k < name.size(); k++) {
    char32_t c = t[i + k];
    if (c >= 0x80) {
      if (folds_to_ascii(c)) throw NonAscii("a character re.I may fold onto ASCII, in a name compared");
      return false;
    }
    if (ascii_lower(c) != ascii_lower(name[k])) return false;
  }
  return true;
}

bool same(const U32& t, size_t i, const U32& lit) { return i + lit.size() <= t.size() && t.compare(i, lit.size(), lit) == 0; }

// \s*"([^"]*)" after an '=': the value's span and the end, or nothing.
bool quoted_value(const U32& t, size_t i, Span& value, size_t& end) {
  i = skip_spaces(t, i);
  if (i >= t.size() || t[i] != '"') return false;
  const size_t close = t.find(U'"', i + 1);
  if (close == U32::npos) return false;
  value = {i + 1, close};
  end = close + 1;
  return true;
}

}  // namespace

U32 from_ascii(const char* s) {
  U32 out;
  for (; *s; s++) out += char32_t((unsigned char)*s);
  return out;
}

U32 from_utf8(const std::string& s) {
  U32 out;
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  size_t n = s.size(), i = 0;
  while (i < n) {
    unsigned c = p[i];
    if (c < 0x80) {
      out += char32_t(c);
      i++;
      continue;
    }
    int len = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xC2 && c < 0xE0 ? 2 : 0;
    if (!len || i + len > n) throw Error("UTF-8: invalid start byte or unexpected end");
    char32_t u = len == 2 ? c & 0x1F : len == 3 ? c & 0x0F : c & 0x07;
    for (int k = 1; k < len; k++) {
      unsigned d = p[i + k];
      if ((d & 0xC0) != 0x80) throw Error("UTF-8: invalid continuation byte");
      u = u << 6 | (d & 0x3F);
    }
    if ((len == 3 && (u < 0x800 || (u >= 0xD800 && u < 0xE000))) || (len == 4 && (u < 0x10000 || u > 0x10FFFF)))
      throw Error("UTF-8: invalid encoding");
    out += u;
    i += size_t(len);
  }
  return out;
}

std::string to_utf8(const U32& s) {
  std::string out;
  for (char32_t c : s) utf8_out(out, c);
  return out;
}

Decoded decode_xml(const Bytes& d) {
  if (d.size() >= 2 && ((d[0] == 0xFF && d[1] == 0xFE) || (d[0] == 0xFE && d[1] == 0xFF)))
    return {utf16_decode(d.data() + 2, d.size() - 2, d[0] == 0xFF), Codec::Utf16};
  if (d.size() >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF)
    return {from_utf8(std::string(d.begin() + 3, d.end())), Codec::Utf8Sig};
  return {from_utf8(std::string(d.begin(), d.end())), Codec::Utf8};
}

Bytes encode(const U32& text, Codec codec) {
  Bytes out;
  if (codec == Codec::Utf16) {
    out = {0xFF, 0xFE};   // Python's "utf-16": a BOM, then the machine's order (little endian)
    for (char32_t c : text) {
      if (c >= 0x10000) {
        const char32_t v = c - 0x10000, hi = 0xD800 + (v >> 10), lo = 0xDC00 + (v & 0x3FF);
        out.insert(out.end(), {uint8_t(hi), uint8_t(hi >> 8), uint8_t(lo), uint8_t(lo >> 8)});
      } else {
        out.insert(out.end(), {uint8_t(c), uint8_t(c >> 8)});
      }
    }
    return out;
  }
  if (codec == Codec::Utf8Sig) out = {0xEF, 0xBB, 0xBF};
  const std::string s = to_utf8(text);
  out.insert(out.end(), s.begin(), s.end());
  return out;
}

bool py_isspace(char32_t c) {
  return (c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0 || c == 0x1680 ||
         (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

bool py_isword(char32_t c) {
  if (c >= 0x80) throw NonAscii("re's \\w or \\b decided on a character outside ASCII");
  return ascii_alnum(c) || c == '_';
}

// str.isprintable: ASCII exactly; outside it, only where the answer is known without Unicode's
// tables: Latin-1's letters and signs (U+00A1-U+00FF, but the soft hyphen U+00AD, not printable)
// and the common punctuation U+2010-U+2027, U+2030-U+205E (printable). Anything else outside ASCII
// stops (NonAscii): Python would decide by its tables (the review: unassigned and format
// characters).
bool py_isprintable(char32_t c) {
  if (c < 0x80) return c >= 0x20 && c < 0x7F;
  if (c == 0xAD) return false;
  if ((c >= 0xA1 && c <= 0xFF) || (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205E)) return true;
  throw NonAscii("str.isprintable() on a character outside the ranges known here");
}

U32 py_lower_ascii(const U32& s) {
  U32 out = s;
  for (char32_t& c : out) {
    if (c >= 0x80) throw NonAscii("a path outside ASCII lowercased");
    c = ascii_lower(c);
  }
  return out;
}

U32 substr(const U32& s, const Span& span) { return s.substr(span.begin, span.end - span.begin); }

std::vector<Match> find_tags(const U32& t, const U32& element) {
  std::vector<Match> out;
  const U32 open = U"<" + element;
  size_t i = 0;
  while ((i = t.find(open, i)) != U32::npos) {
    const size_t after = i + open.size();
    if (after < t.size() && py_isword(t[after])) {   // \b after the element's name
      i++;
      continue;
    }
    const size_t close = t.find(U'>', after);
    if (close == U32::npos) break;                    // no '>' further on: none at any later '<' either
    out.push_back({{i, close + 1}, {}});
    i = close + 1;
  }
  return out;
}

U32 retag(const U32& t, const U32& element, const std::function<U32(const U32&)>& fn) {
  U32 out;
  size_t last = 0;
  for (const Match& m : find_tags(t, element)) {
    out += t.substr(last, m.all.begin - last);
    out += fn(substr(t, m.all));
    last = m.all.end;
  }
  return out + t.substr(last);
}

std::optional<Match> find_attr(const U32& t, const U32& name, size_t from) {
  for (size_t i = from; i < t.size(); i++) {
    if (!same_nocase(t, i, name) || !boundary_before(t, i)) continue;
    size_t k = skip_spaces(t, i + name.size());
    if (k >= t.size() || t[k] != '=') continue;
    Span value;
    size_t end;
    if (!quoted_value(t, k + 1, value, end)) continue;
    return Match{{i, end}, {{i, value.begin}, value, {value.end, end}}};
  }
  return std::nullopt;
}

std::optional<U32> attr(const U32& tag, const U32& name) {
  auto m = find_attr(tag, name);
  if (!m) return std::nullopt;
  return substr(tag, m->groups[1]);
}

U32 set_attr(const U32& tag, const U32& name, const U32& value) {
  auto m = find_attr(tag, name);
  if (!m) return tag;
  return tag.substr(0, m->groups[1].begin) + value + tag.substr(m->groups[1].end);
}

U32 sub_all(const U32& t, const std::function<std::optional<Match>(const U32&, size_t)>& find,
            const std::function<U32(const U32&, const Match&)>& fn) {
  U32 out;
  size_t last = 0, at = 0;
  while (at <= t.size()) {
    auto m = find(t, at);
    if (!m) break;
    out += t.substr(last, m->all.begin - last);
    out += fn(t, *m);
    last = at = m->all.end;
  }
  return out + t.substr(last);
}

std::optional<Match> find_set_ref(const U32& t, size_t from) {
  static const U32 lit = U"set:";
  for (size_t i = t.find(lit, from); i != U32::npos; i = t.find(lit, i + 1)) {
    if (!boundary_before(t, i)) continue;
    size_t j = i + lit.size();
    while (j < t.size() && !py_isspace(t[j]) && t[j] != '"') j++;
    if (j == i + lit.size()) continue;
    return Match{{i, j}, {{i, i + lit.size()}, {i + lit.size(), j}}};
  }
  return std::nullopt;
}

std::optional<Match> find_font_prop(const U32& t, size_t from) {
  static const U32 ame = U"ame", alue = U"alue", font = U"\"Font\"";
  for (size_t i = from; i + 4 <= t.size(); i++) {
    if ((t[i] != 'N' && t[i] != 'n') || !same(t, i + 1, ame) || !boundary_before(t, i)) continue;
    size_t k = skip_spaces(t, i + 4);
    if (k >= t.size() || t[k] != '=') continue;
    k = skip_spaces(t, k + 1);
    if (!same(t, k, font)) continue;
    k += font.size();
    const size_t spaces = k;
    k = skip_spaces(t, k);
    if (k == spaces || k >= t.size() || (t[k] != 'V' && t[k] != 'v') || !same(t, k + 1, alue)) continue;
    k = skip_spaces(t, k + 5);
    if (k >= t.size() || t[k] != '=') continue;
    Span value;
    size_t end;
    if (!quoted_value(t, k + 1, value, end)) continue;
    return Match{{i, end}, {{i, value.begin}, value, {value.end, end}}};
  }
  return std::nullopt;
}

size_t count_window_tags(const U32& t) {
  static const U32 lit = U"<Window";
  size_t n = 0;
  for (size_t i = t.find(lit); i != U32::npos; i = t.find(lit, i + 1)) {
    const size_t after = i + lit.size();
    if (after >= t.size() || !py_isword(t[after])) n++;
  }
  return n;
}

std::map<U32, U32> word_attrs(const U32& tag) {
  std::map<U32, U32> out;
  size_t i = 0;
  while (i < tag.size()) {
    // \w+ as a run (a character outside ASCII taken as a word one: see the throw below).
    auto word = [](char32_t c) { return c >= 0x80 || ascii_alnum(c) || c == '_'; };
    if (!word(tag[i])) {
      i++;
      continue;
    }
    size_t j = i;
    while (j < tag.size() && word(tag[j])) j++;
    size_t k = skip_spaces(tag, j);
    Span value;
    size_t end;
    if (k < tag.size() && tag[k] == '=' && quoted_value(tag, k + 1, value, end)) {
      for (size_t q = (i ? i - 1 : 0); q < j; q++)
        if (tag[q] >= 0x80) throw NonAscii("an attribute's name next to a character outside ASCII");
      out[tag.substr(i, j - i)] = substr(tag, value);
      i = end;
      continue;
    }
    i++;   // the next position (inside the same run: it fails the same way, as re's scan does)
  }
  return out;
}

std::optional<U32> imagefile_value(const U32& t) {
  static const U32 lit = U"Imagefile";
  for (size_t i = t.find(lit); i != U32::npos; i = t.find(lit, i + 1)) {
    size_t k = skip_spaces(t, i + lit.size());
    if (k >= t.size() || t[k] != '=') continue;
    Span value;
    size_t end;
    if (quoted_value(t, k + 1, value, end)) return substr(t, value);
  }
  return std::nullopt;
}

std::optional<long long> py_int(const U32& s) {
  size_t b = 0, e = s.size();
  while (b < e && py_isspace(s[b])) b++;
  while (e > b && py_isspace(s[e - 1])) e--;
  bool neg = false;
  if (b < e && (s[b] == '+' || s[b] == '-')) neg = s[b++] == '-';
  if (b >= e) return std::nullopt;
  long long v = 0;
  bool prev_digit = false;
  for (size_t i = b; i < e; i++) {
    if (s[i] == '_' && prev_digit && i + 1 < e && s[i + 1] >= '0' && s[i + 1] <= '9') { prev_digit = false; continue; }
    if (s[i] >= 0x80) throw NonAscii("int() on a character outside ASCII (Python takes Unicode digits)");
    if (s[i] < '0' || s[i] > '9') return std::nullopt;
    v = v * 10 + (s[i] - '0');
    prev_digit = true;
  }
  return neg ? -v : v;
}

}  // namespace x360ui
