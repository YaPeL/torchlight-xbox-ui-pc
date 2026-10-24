# Third-party code in the installer

- **tiny-AES-c** (`tiny-aes/aes.c`, `aes.h`): https://github.com/kokke/tiny-AES-c at commit
  `23856752fbd139da0b8ca6e471a13d5bcc99a08d`, unchanged. Released into the public domain under
  The Unlicense (`tiny-aes/unlicense.txt`). Built with CBC only, AES-128.
- **LZX** (`../src/lzx.cpp`, `lzx.h`): a C++ port of `tools/xex/lzx_decompress.py`. That file mirrors
  libmspack's LZX decoder (its state, its tables, its frame by frame decoding, its Huffman tables'
  build), so both are taken as derived from it: **libmspack**, https://github.com/kyz/libmspack,
  `libmspack/mspack/lzxd.c` with `lzx.h` and `readhuff.h`,
  (C) 2003-2023 Stuart Caie (the LZX method by Jonathan Forbes and Tomi Poutanen, adapted by
  Microsoft), originally under the GNU Lesser General Public License 2.1 (`LGPL-2.1.txt`, beside
  this file); this copy is under GPL-3.0 (`LICENSE`), as its section 3 allows, and its source is
  this repository's at the release's tag. The Python file itself comes from
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
- **miniz** (`miniz/miniz.c`, `miniz.h`): https://github.com/richgel999/miniz release 3.1.2
  (`miniz-3.1.2.zip`, SHA-256 `f0446d86…a140d9a`), unchanged. MIT License (`miniz/LICENSE`):
  Copyright 2013-2014 RAD Game Tools and Valve Software; Copyright 2010-2014 Rich Geldreich and
  Tenacious Software LLC. Used for reading the zip files and for inflate (the icons' PNG data).
- **The pad prompts' icons** (`../../data/prompts`, built into the executable): Kenney's "Input
  Prompts", CC0 (`data/prompts/LICENSE.txt`).

The Linux release's AppImage also carries the AppImage runtime, a separate program that mounts it:
its notices are `packaging/linux/APPIMAGE_NOTICES.md`.
