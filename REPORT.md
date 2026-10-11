# REPORT.md – Programming Assignment 02 (ls utility)

## Feature 2 – Long Listing (v1.1.0)

**Q1. Difference between `stat()` and `lstat()`; when is `lstat()` appropriate in `ls`?**
`stat()` follows symbolic links and returns information about the file the link points to. `lstat()` does not follow the link; it returns information about the link itself (type `S_IFLNK`, size = length of the target path). `ls -l` must use `lstat()` because it has to show a symlink as a symlink (`l` in the type field, the link's own permissions/size, and `name -> target`). `stat()` would hide the link and describe its target (and fail on dangling links). `lstat()` is also what prevents infinite loops in recursive listing, since symlinks to directories are not followed.

**Q2. Extracting file type and permissions from `st_mode` using bitwise operators and macros.**
`st_mode` packs the file type in the high bits and permissions in the low 12 bits. The type is extracted by masking with `S_IFMT` and comparing: `(st_mode & S_IFMT) == S_IFDIR` (the macro `S_ISDIR(m)` does exactly this; likewise `S_ISREG`, `S_ISLNK`, ...). Permissions are tested by AND-ing with a single-bit mask: `st_mode & S_IRUSR` is non-zero if the owner can read; similarly `S_IWUSR`, `S_IXUSR`, `S_IRGRP`, ..., `S_IXOTH`. Special bits `S_ISUID`, `S_ISGID`, `S_ISVTX` are tested the same way and change the execute character to `s/S` or `t/T`. In the code, `mode_to_string()` builds the `drwxr-xr-x` string with one test per character.
## Feature 3 – Column Display (v1.2.0)

**Q1. Logic of "down then across"; why isn't a single loop enough?**
Names are sorted and laid out column by column: first `rows` names go in column 0, the next `rows` in column 1, and so on. Since printing happens line by line, we need `rows = ceil(n / cols)`, and for row `r` and column `c` the item is at index `r + c * rows`. A single loop over the array would print in the order 0,1,2,... across a row, which gives "across" ordering, not "down". The terminal can only print left-to-right, top-to-bottom, so we must jump through the array with a stride of `rows`.

**Q2. Purpose of `ioctl` and limits of a fixed 80-column fallback.**
`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the terminal driver for its current size (`ws.ws_col`, `ws.ws_row`). With only a fixed 80 columns, the output would waste space on wide terminals, and on narrow terminals the lines would wrap and break the column alignment. It also would not adapt when the window is resized. The code uses 80 only if `ioctl` fails (e.g., output not a terminal).
