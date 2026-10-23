# Third-party code in the installer

- **tiny-AES-c** (`tiny-aes/aes.c`, `aes.h`): https://github.com/kokke/tiny-AES-c at commit
  `23856752fbd139da0b8ca6e471a13d5bcc99a08d`, unchanged. Released into the public domain under
  The Unlicense (`tiny-aes/unlicense.txt`). Built with CBC only, AES-128.
- **LZX** (`../src/lzx.cpp`, `lzx.h`): a C++ port of `tools/xex/lzx_decompress.py`, which comes from
  https://github.com/sp00nznet/360tools (`tools/lzx_decompress.py`), MIT License:

  > Copyright (c) 2026 sp00nznet
  >
  > Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  > and associated documentation files (the "Software"), to deal in the Software without
  > restriction, including without limitation the rights to use, copy, modify, merge, publish,
  > distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
  > Software is furnished to do so, subject to the following conditions:
  >
  > The above copyright notice and this permission notice shall be included in all copies or
  > substantial portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  > BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  > NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  > DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  > OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
