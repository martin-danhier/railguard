# Railguard

3D game engine using Vulkan and SDL2 - V3

## Configure development environment

1. Clone this repository with the `--recurse-submodules` flag.
    - If you forgot the flag, you can run `git submodule update --init` instead.
2. Install Vulkan SDK. Make sure to have the latest version, since vk_mem_alloc is often up-to-date.
3. Install SDL2
4. Install a C++ compiler (on Linux, clang and ninja are recommended).

## Enable GDB pretty print

To enable GDB pretty print, add this line to your `~/.gdbinit` file:

```gdb
source <repo_dir>/gdb_formatter.py
```

where you replace `<repo_dir>` by the directory where you cloned this repository.

This will allow GDB to use custom pretty printers for core railguard types, such as `Vector`.