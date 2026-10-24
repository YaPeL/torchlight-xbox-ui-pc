# Writes OUT, a C++ file with the bytes of FILE as NAME (and NAME_SIZE), or an empty one when FILE
# is empty: the proxies a release builds in; proxy/cegui_exports.txt, LICENSE and the notices as
# text (TEXT=1: a C string, without CRs).
if(FILE AND EXISTS "${FILE}")
  file(READ "${FILE}" hex HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
  set(size_expr "sizeof data")
else()
  set(hex "0")
  set(size_expr "0")
endif()
if(TEXT)
  # Without CRs: a Windows checkout's CRLF would print as CR CR LF through a text-mode stream.
  string(REPLACE "0x0d," "" hex "${hex}")
  # Each byte as a char literal ('\xNN'): a UTF-8 text's bytes over 0x7f fit char without a cast.
  string(REGEX REPLACE "0x([0-9a-f][0-9a-f])," "'\\\\x\\1'," hex "${hex}")
  file(WRITE "${OUT}" "namespace x360ui {\nextern const char ${NAME}[];\nconst char ${NAME}[] = {${hex}0};\n}\n")
else()
  file(WRITE "${OUT}" "#include <cstddef>\nnamespace x360ui {\nnamespace { const unsigned char data[] = {${hex}}; }\nextern const unsigned char* const ${NAME};\nextern const size_t ${NAME}Size;\nconst unsigned char* const ${NAME} = data;\nconst size_t ${NAME}Size = ${size_expr};\n}\n")
endif()
