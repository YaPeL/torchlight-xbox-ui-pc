// Text as tools/convert_x360_ui.py handles it: XML files decoded as Python does (UTF-16 with a
// BOM, UTF-8 with or without one), encoded back the same way, and the few re patterns it uses
// matched by hand with Python's rules. Where Python's answer depends on Unicode tables this code
// does not carry (\w, \b and re.I on characters outside ASCII), it stops with NonAscii instead of
// guessing (the owner: a difference must not pass silently).
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bytes.h"

namespace x360ui {

using U32 = std::u32string;

struct NonAscii : Error {
  using Error::Error;
};

enum class Codec { Utf16, Utf8Sig, Utf8 };

struct Decoded {
  U32 text;
  Codec codec;
};

Decoded decode_xml(const Bytes& data);                 // convert_x360_ui.decode
Bytes encode(const U32& text, Codec codec);            // str.encode(codec)
U32 from_utf8(const std::string& s);                   // strict, as bytes.decode("utf-8")
std::string to_utf8(const U32& s);
U32 from_ascii(const char* s);

bool py_isspace(char32_t c);                           // str.isspace / re's \s on str
bool py_isword(char32_t c);                            // re's \w on str (NonAscii outside ASCII)
bool py_isprintable(char32_t c);                       // str.isprintable for one character (see the .cpp)
U32 py_lower_ascii(const U32& s);                      // str.lower() on an ASCII path (NonAscii else)

// One match of a pattern: [begin, end) and its groups' spans.
struct Span {
  size_t begin = 0, end = 0;
};
struct Match {
  Span all;
  std::vector<Span> groups;
};

// <ELEMENT\b[^>]*> (case-sensitive), every match left to right.
std::vector<Match> find_tags(const U32& text, const U32& element);
// re.sub(TAG.format(element), fn, text).
U32 retag(const U32& text, const U32& element, const std::function<U32(const U32&)>& fn);
// (\bNAME\s*=\s*")([^"]*)(") with re.I: the first match from `from`, if any.
std::optional<Match> find_attr(const U32& text, const U32& name, size_t from = 0);
std::optional<U32> attr(const U32& tag, const U32& name);
U32 set_attr(const U32& tag, const U32& name, const U32& value);
// Every non-overlapping match of a finder, left to right, replaced by fn (re.sub).
U32 sub_all(const U32& text, const std::function<std::optional<Match>(const U32&, size_t)>& find,
            const std::function<U32(const U32&, const Match&)>& fn);
std::optional<Match> find_set_ref(const U32& text, size_t from);      // (\bset:)([^\s"]+)
std::optional<Match> find_font_prop(const U32& text, size_t from);    // (\b[Nn]ame\s*=\s*"Font"\s+[Vv]alue\s*=\s*")([^"]*)(")
size_t count_window_tags(const U32& text);                            // len(re.findall(r"<Window\b", text))
// re.findall(r'(\w+)\s*=\s*"([^"]*)"', tag) as a dict (the last of a key kept).
std::map<U32, U32> word_attrs(const U32& tag);
std::optional<U32> imagefile_value(const U32& xml);                   // re.search(r'Imagefile\s*=\s*"([^"]*)"')
std::optional<long long> py_int(const U32& s);                        // int(s), None on ValueError

U32 substr(const U32& s, const Span& span);

}  // namespace x360ui
