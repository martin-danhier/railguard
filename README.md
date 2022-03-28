# Railguard

3D game engine using Vulkan and SDL2 - V3

## Enable GDB pretty print

To enable GDB pretty print, add this line to your `~/.gdbinit` file:

```gdb
source <repo_dir>/gdb_formatter.py
```

where you replace `<repo_dir>` by the directory where you cloned this repository.

This will allow GDB to use custom pretty printers for core railguard types, such as `Vector`.