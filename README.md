# Microsoft Rich Signature Remover (aka RichKiller)

**RichKiller** is a **native** utility for removing the **Rich header** from PE files (EXE/DLL) produced by MSVC and other Microsoft toolchains by **relocating the PE header**.

> ⚠️ Use this tool **only** on binaries that you own or have a legal right to modify.

---

## What it does

The tool does **not** simply zero out the Rich area.  
Instead, it performs a structural transformation of the PE header layout:

1. Opens a PE file (`.exe` / `.dll`) on disk.
2. Parses the DOS header and reads `e_lfanew` (the original PE header offset).
3. Identifies the region between the DOS stub and the original PE header (where the Rich header resides in MSVC-compiled binaries).
4. **Moves the entire PE header (NT headers + Optional header + Section headers) to the beginning of that region**, overwriting the Rich header (and partially or completely overwriting the DOS stub).
5. Updates `e_lfanew` so it points to the new PE header location.
6. Optionally zeroes the old PE header region to keep the layout clean.

As a result:

- The DOS header remains valid (`MZ` and a correct `e_lfanew`).
- The PE header remains valid but is located at an earlier file offset.
- The **Rich header is completely removed** (overwritten by the PE header).
- The DOS stub is shortened or removed (the `"This program cannot be run in DOS mode"` message may be partially or fully destroyed).
- The file remains fully loadable by the Windows loader; the PE structure is consistent.

⚠️ **Important:** Any existing Authenticode (digital) signature will be invalidated because the file contents are modified after signing.

---

## Typical use cases

- Reproducible builds and binary comparison (removing toolchain-specific metadata).
- Reducing leakage of toolchain and build-environment fingerprints (metadata stored in the Rich header).
- Forensics and research on PE structure and compiler/linker fingerprints.

This tool is **not** intended to bypass copy protection, licensing systems, or any access control mechanisms.

---

## Features

- **Native implementation:**  
  Built directly on top of NT/Win32 APIs, with no dependency on the C runtime in the reference build (CRT-free).
- Works directly on files on disk; no mandatory temporary copies.
- Performs a deterministic structural transformation:
  - relocates the PE header into the Rich region,
  - updates `e_lfanew`,
  - preserves consistency between `SizeOfHeaders`, the section table, and `PointerToRawData`.
- Suitable for integration as a **post-build step** in Visual Studio or other build systems.
- Supports x86 and x64 PE files (regardless of what architecture the tool itself is created for).

---

## Usage

```bash
richkiller [-i file] -b -r
Options:
  -i file  executables to remove rich signature
  -b       backup copy
  -r       add checksum to header
