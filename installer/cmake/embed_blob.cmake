# Writes OUT, a C++ file with the bytes of FILE as NAME (and NAME_SIZE), or an empty one when FILE
# is empty: the proxies a release builds in, proxy/cegui_exports.txt as text (TEXT=1: a C string).
if(FILE AND EXISTS "${FILE}")
  file(READ "${FILE}" hex HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
  set(size_expr "sizeof data")
else()
  set(hex "0")
  set(size_expr "0")
endif()
if(TEXT)
  # Each byte as a char: a UTF-8 text's bytes over 0x7f do not narrow-convert into char by themselves.
  string(REPLACE "0x" "(char)0x" hex "${hex}")
  file(WRITE "${OUT}" "namespace x360ui {\nextern const char ${NAME}[];\nconst char ${NAME}[] = {${hex}0};\n}\n")
else()
  file(WRITE "${OUT}" "#include <cstddef>\nnamespace x360ui {\nnamespace { const unsigned char data[] = {${hex}}; }\nextern const unsigned char* const ${NAME};\nextern const size_t ${NAME}Size;\nconst unsigned char* const ${NAME} = data;\nconst size_t ${NAME}Size = ${size_expr};\n}\n")
endif()
