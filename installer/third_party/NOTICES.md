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
  (`miniz-3.1.2.zip`, SHA-256 `f0446d86…a140d9a`), unchanged. Used for reading the zip files and
  for inflate (the icons' PNG data). MIT License (`miniz/LICENSE`):

  > Copyright 2013-2014 RAD Game Tools and Valve Software
  > Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
  >
  > All Rights Reserved.
  >
  > Permission is hereby granted, free of charge, to any person obtaining a copy
  > of this software and associated documentation files (the "Software"), to deal
  > in the Software without restriction, including without limitation the rights
  > to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  > copies of the Software, and to permit persons to whom the Software is
  > furnished to do so, subject to the following conditions:
  >
  > The above copyright notice and this permission notice shall be included in
  > all copies or substantial portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  > AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  > OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  > THE SOFTWARE.
- **The pad prompts' icons** (`../../data/prompts`, built into the executable): Kenney's "Input
  Prompts", CC0 (`data/prompts/LICENSE.txt`).

- **SDL 3.4.14** (the wizard's window, renderer, pads and file pickers; fetched by
  `installer/cmake/wizard_deps.cmake`, built in), zlib licence. The HIDAPI it includes is used
  under its GPL-3.0 option.

  > Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>
  >
  > This software is provided 'as-is', without any express or implied
  > warranty.  In no event will the authors be held liable for any damages
  > arising from the use of this software.
  >
  > Permission is granted to anyone to use this software for any purpose,
  > including commercial applications, and to alter it and redistribute it
  > freely, subject to the following restrictions:
  >
  > 1. The origin of this software must not be misrepresented; you must not
  >    claim that you wrote the original software. If you use this software
  >    in a product, an acknowledgment in the product documentation would be
  >    appreciated but is not required.
  > 2. Altered source versions must be plainly marked as such, and must not be
  >    misrepresented as being the original software.
  > 3. This notice may not be removed or altered from any source distribution.
- **Dear ImGui 1.92.5** (the wizard's widgets, with its SDL3 and SDL_Renderer backends; fetched,
  built in), MIT:

  > Copyright (c) 2014-2025 Omar Cornut
  >
  > Permission is hereby granted, free of charge, to any person obtaining a copy
  > of this software and associated documentation files (the "Software"), to deal
  > in the Software without restriction, including without limitation the rights
  > to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  > copies of the Software, and to permit persons to whom the Software is
  > furnished to do so, subject to the following conditions:
  >
  > The above copyright notice and this permission notice shall be included in all
  > copies or substantial portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  > AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  > OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  > SOFTWARE.
- **Noto Sans 2.015** (the wizard's font, regular and bold; fetched, built in): Copyright 2022 The
  Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic), SIL Open Font License
  1.1, whose full text is `OFL-1.1.txt` beside this file (and in `x360ui-install notices`).
- **The wizard's colours** (`installer/src/wizard/app.cpp`, `apply_theme`) are the ReXGlue SDK's
  ImGui palette (its `src/ui/imgui_drawer.cpp`), BSD-3-Clause:

  > Copyright (c) 2026, Tom Clay <tomc@tctechstuff.com>
  >
  > Portions of this software are derived from the Xenia project:
  > Copyright (c) 2022, Ben Vanik and Xenia project contributors
  >
  > Redistribution and use in source and binary forms, with or without
  > modification, are permitted provided that the following conditions are met:
  >
  > 1. Redistributions of source code must retain the above copyright notice, this
  >    list of conditions and the following disclaimer.
  > 2. Redistributions in binary form must reproduce the above copyright notice,
  >    this list of conditions and the following disclaimer in the documentation
  >    and/or other materials provided with the distribution.
  > 3. Neither the name of the copyright holder nor the names of its
  >    contributors may be used to endorse or promote products derived from
  >    this software without specific prior written permission.
  >
  > THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  > AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  > IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  > DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  > FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  > DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  > SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  > CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  > OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  > OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The Linux release's AppImage also carries the AppImage runtime, a separate program that mounts it:
its notices are `packaging/linux/APPIMAGE_NOTICES.md`.
