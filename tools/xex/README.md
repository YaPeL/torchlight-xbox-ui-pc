The 360 executable's image, decrypted and decompressed, for the converter's strings (read by image
address: guest address A is file offset A - base).

- `xex_dump.py`: from the owner's own torchlight-recomp repository (tools/xex/xex_dump.py),
  GPL-3.0 like this repository. Usage: `python3 tools/xex/xex_dump.py default.xex default.bin`
  (pycryptodome when installed, else `aes128.py`, plain Python AES written here). Changed here: the AES fallback.
- `lzx_decompress.py`: from https://github.com/sp00nznet/360tools (tools/lzx_decompress.py),
  MIT License, Copyright (c) 2026 sp00nznet; its notice is at the top of the file and in `LICENSE-lzx_decompress`.
