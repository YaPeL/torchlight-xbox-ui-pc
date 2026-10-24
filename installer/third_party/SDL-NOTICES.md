# SDL 3.4.14's own third-party code

What the static SDL in the installer carries besides SDL itself (its zlib licence is in
`NOTICES.md`), from SDL's source tree as the installer builds it (`installer/cmake/wizard_deps.cmake`:
no audio, camera, GPU, KMSDRM or Vulkan).

- **HIDAPI** (`src/hidapi`): used under its GPL-3.0 option (the installer is GPL-3.0); its
  copyright: Alan Ott, Signal 11 Software, and the HIDAPI contributors.

- **yuv2rgb** (`src/video/yuv2rgb`), BSD-3-Clause:

  > Copyright (c) 2016, Adrien Descamps
  > All rights reserved.
  >
  > Redistribution and use in source and binary forms, with or without
  > modification, are permitted provided that the following conditions are met:
  >
  > * Redistributions of source code must retain the above copyright notice, this
  >   list of conditions and the following disclaimer.
  >
  > * Redistributions in binary form must reproduce the above copyright notice,
  >   this list of conditions and the following disclaimer in the documentation
  >   and/or other materials provided with the distribution.
  >
  > * Neither the name of yuv2rgb nor the names of its
  >   contributors may be used to endorse or promote products derived from
  >   this software without specific prior written permission.
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

- **The math functions** (`src/libm`, from FreeBSD's msun):

  > Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
  >
  > Developed at SunPro, a Sun Microsystems, Inc. business.
  > Permission to use, copy, modify, and distribute this
  > software is freely granted, provided that this notice
  > is preserved.

- **stb_image** (`src/video/stb_image.h`, Sean Barrett and contributors): public domain (or MIT, at
  the user's choice).

- **The Wayland protocols** (`wayland-protocols/*.xml`, compiled by wayland-scanner into SDL):

  - `alpha-modifier-v1.xml`: Copyright © 2024 Xaver Hugl
  - `color-management-v1.xml`: Copyright 2019 Sebastian Wick; Copyright 2019 Erwin Burema; Copyright 2020 AMD; Copyright 2020-2024 Collabora, Ltd.; Copyright 2024 Xaver Hugl; Copyright 2022-2025 Red Hat, Inc.
  - `fractional-scale-v1.xml`: Copyright © 2022 Kenny Levinsen
  - `frog-color-management-v1.xml`: Copyright © 2023 Joshua Ashton for Valve Software; Copyright © 2023 Xaver Hugl
  - `idle-inhibit-unstable-v1.xml`: Copyright © 2015 Samsung Electronics Co., Ltd
  - `input-timestamps-unstable-v1.xml`: Copyright © 2017 Collabora, Ltd.
  - `keyboard-shortcuts-inhibit-unstable-v1.xml`: Copyright © 2017 Red Hat Inc.
  - `pointer-constraints-unstable-v1.xml`: Copyright © 2014      Jonas Ådahl; Copyright © 2015      Red Hat Inc.
  - `primary-selection-unstable-v1.xml`: Copyright © 2015, 2016 Red Hat
  - `relative-pointer-unstable-v1.xml`: Copyright © 2014      Jonas Ådahl; Copyright © 2015      Red Hat Inc.
  - `viewporter.xml`: Copyright © 2013-2016 Collabora, Ltd.
  - `xdg-activation-v1.xml`: Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>; Copyright © 2020 Carlos Garnacho <carlosg@gnome.org>
  - `xdg-decoration-unstable-v1.xml`: Copyright © 2018 Simon Ser
  - `xdg-dialog-v1.xml`: Copyright © 2023 Carlos Garnacho
  - `xdg-foreign-unstable-v2.xml`: Copyright © 2015-2016 Red Hat Inc.
  - `xdg-output-unstable-v1.xml`: Copyright © 2017 Red Hat Inc.
  - `xdg-shell.xml`: Copyright © 2008-2013 Kristian Høgsberg; Copyright © 2013      Rafael Antognolli; Copyright © 2013      Jasper St. Pierre; Copyright © 2010-2013 Intel Corporation; Copyright © 2015-2017 Samsung Electronics Co., Ltd; Copyright © 2015-2017 Red Hat Inc.
  - `xdg-toplevel-icon-v1.xml`: Copyright © 2023-2024 Matthias Klumpp; Copyright ©      2024 David Edmundson

  > Permission is hereby granted, free of charge, to any person obtaining a
  > copy of this software and associated documentation files (the "Software"),
  > to deal in the Software without restriction, including without limitation
  > the rights to use, copy, modify, merge, publish, distribute, sublicense,
  > and/or sell copies of the Software, and to permit persons to whom the
  > Software is furnished to do so, subject to the following conditions:
  >
  > The above copyright notice and this permission notice (including the next
  > paragraph) shall be included in all copies or substantial portions of the
  > Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  > THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  > FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  > DEALINGS IN THE SOFTWARE.

  - `cursor-shape-v1.xml`: Copyright 2018 The Chromium Authors; Copyright 2023 Simon Ser
  - `pointer-warp-v1.xml`: Copyright © 2024 Neal Gompa; Copyright © 2024 Xaver Hugl; Copyright © 2024 Matthias Klumpp; Copyright © 2024 Vlad Zahorodnii

  > Permission is hereby granted, free of charge, to any person obtaining a
  > copy of this software and associated documentation files (the "Software"),
  > to deal in the Software without restriction, including without limitation
  > the rights to use, copy, modify, merge, publish, distribute, sublicense,
  > and/or sell copies of the Software, and to permit persons to whom the
  > Software is furnished to do so, subject to the following conditions:
  > The above copyright notice and this permission notice (including the next
  > paragraph) shall be included in all copies or substantial portions of the
  > Software.
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  > THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  > FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  > DEALINGS IN THE SOFTWARE.

  - `tablet-v2.xml`: Copyright 2014 © Stephen "Lyude" Chandler Paul; Copyright 2015-2016 © Red Hat, Inc.
  - `wayland.xml`: Copyright © 2008-2011 Kristian Høgsberg; Copyright © 2010-2011 Intel Corporation; Copyright © 2012-2013 Collabora, Ltd.

  > Permission is hereby granted, free of charge, to any person
  > obtaining a copy of this software and associated documentation files
  > (the "Software"), to deal in the Software without restriction,
  > including without limitation the rights to use, copy, modify, merge,
  > publish, distribute, sublicense, and/or sell copies of the Software,
  > and to permit persons to whom the Software is furnished to do so,
  > subject to the following conditions:
  >
  > The above copyright notice and this permission notice (including the
  > next paragraph) shall be included in all copies or substantial
  > portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  > EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  > MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  > NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  > BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  > ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  > CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  > SOFTWARE.

  - `text-input-unstable-v3.xml`: Copyright © 2012, 2013 Intel Corporation; Copyright © 2015, 2016 Jan Arne Petersen; Copyright © 2017, 2018 Red Hat, Inc.; Copyright © 2018       Purism SPC

  > Permission to use, copy, modify, distribute, and sell this
  > software and its documentation for any purpose is hereby granted
  > without fee, provided that the above copyright notice appear in
  > all copies and that both that copyright notice and this permission
  > notice appear in supporting documentation, and that the name of
  > the copyright holders not be used in advertising or publicity
  > pertaining to distribution of the software without specific,
  > written prior permission.  The copyright holders make no
  > representations about the suitability of this software for any
  > purpose.  It is provided "as is" without express or implied
  > warranty.
  >
  > THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
  > SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
  > FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
  > SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  > WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
  > AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  > ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
  > THIS SOFTWARE.

- **The Khronos headers** for EGL and OpenGL ES (`src/video/khronos`), MIT: Copyright 2013-2020 The
  Khronos Group Inc.
