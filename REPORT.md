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
## Feature 4 – Horizontal Display (v1.3.0)
**Q1. Complexity: vertical vs horizontal.**
The vertical ("down then across") version needs more pre-calculation: number of columns, then number of rows (ceiling division), and index mapping `r + c*rows`; the layout of every item depends on the total count. The horizontal version needs only the column width and the current cursor position; each name is printed in order and a newline is emitted when the next column would overflow the terminal width.

**Q2. Managing display modes.**
A global `enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL } display_mode` is set in the `getopt()` loop (`-l` → long, `-x` → horizontal). After reading and sorting the names, `do_ls()` uses a `switch (display_mode)` to call `print_long()`, `print_horizontal()`, or `print_down_across()` (default). The recursion flag `-R` is independent and stored in a separate variable.

## Feature 5 – Alphabetical Sort (v1.4.0)

### Q1. Necessity of reading all directory entries into memory before sorting and drawbacks
- **Necessity:** Directory streams read via `opendir()`/`readdir()` return entries in the arbitrary order in which they are stored in the filesystem's directory data structures (e.g., inode tables or extents), which is not sorted. Because sorting algorithms require global knowledge of all elements to establish comparative order, the program must collect all entry names into memory before it can sort them and calculate row/column layouts.
- **Drawbacks for Millions of Files:**
  1. **High Memory Overhead:** Allocating memory for millions of filename strings (alongside pointer arrays) can consume gigabytes of RAM, risking Out-Of-Memory (OOM) process termination.
  2. **Latency & I/O Delay:** Reading all directory records up front creates noticeable execution lag before any output is displayed to the screen, degrading user responsiveness.

### Q2. Purpose and signature of the qsort() comparison function
- **Function Signature:** `int compare_strings(const void *a, const void *b)`
- **Purpose:** `qsort()` is generic and does not know the data type of the elements in the array. The comparison function tells `qsort()` how to evaluate the order of two elements:
  - Returns `< 0` if element `a` should come before `b`.
  - Returns `0` if elements are equal.
  - Returns `> 0` if element `a` should come after `b`.
- **Why `const void *` Arguments are Required:** `qsort()` operates on generic memory blocks. Passing generic `const void *` pointers allows `qsort()` to remain type-agnostic and work with any arbitrary data structure. Inside the callback, the pointers are cast to `const char **` and dereferenced to retrieve the underlying string addresses passed to `strcmp()`.
## Feature 6 – Colorized Output Based on File Type (`v1.5.0`)

### Q1. ANSI Escape Codes Mechanics
- **Mechanism:** ANSI escape sequences are control character strings starting with `\033[` (Escape) followed by parameters and ending in `m` (Select Graphic Rendition). Terminal emulators process these control sequences to alter text color/attributes rather than displaying the raw characters.
- **Reset Sequence:** `\033[0m` resets terminal attributes to default to avoid bleeding color into subsequent output.
- **Green Code Sequence:** `\033[0;32m` (Standard Green) or `\033[1;32m` (Bold Green).

### Q2. File Type & Executable Bit Evaluation
- **Permission Bit Inspection:** Executable status is checked via bitwise AND against permission mask constants defined in `<sys/stat.h>`:
  - `S_IXUSR` (Owner execute bit)
  - `S_IXGRP` (Group execute bit)
  - `S_IXOTH` (Others execute bit)
- **Evaluation Logic:** `if (st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))` evaluates to true if any execution permission bit is set.
- **File Type Rules:**
  - **Directory:** `S_ISDIR(st_mode)` $\rightarrow$ **Blue** (`\033[1;34m`)
  - **Executable:** `st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)` $\rightarrow$ **Green** (`\033[1;32m`)
  - **Archive/Tarball:** `.tar`, `.gz`, `.zip`, `.tgz` filename extension matching $\rightarrow$ **Red** (`\033[1;31m`)
  - **Symbolic Link:** `S_ISLNK(st_mode)` (via `lstat`) $\rightarrow$ **Pink/Magenta** (`\033[1;35m`)
  - **Special Files:** Character/Block devices, FIFO, Sockets $\rightarrow$ **Reverse Video** (`\033[7m`)

## Feature 7 – Recursive Listing (-R Option) (v1.6.0)

### Q1. Base Case in Recursive Functions and in `ls -R`
- **General Definition:** In recursion, the base case is the terminating condition that stops a function from making further recursive calls to itself, preventing infinite call stack growth and stack overflow errors.
- **`ls -R` Base Case:** The base case occurs when a directory contains no subdirectories (or contains only files, symbolic links, or unreadable entries). Once `do_ls()` iterates through all entries in a directory and finds no child entries satisfying `S_ISDIR(info.st_mode)`, no further recursive `do_ls()` calls are triggered. The function completes execution and pops off the call stack. Skipping `.` and `..` prevents infinite self-referential recursion.

### Q2. Necessity of Constructing Full Relative Paths
- **Necessity:** `opendir()`, `lstat()`, and `stat()` operate relative to the process's Current Working Directory (CWD). Calling `do_ls("subdir")` from inside `do_ls("parent_dir")` fails because the working directory of the process remains set to the root execution directory.
- **What Happens Without Full Pathing:** If `do_ls("subdir")` is called without path prefixing, `opendir("subdir")` attempts to open `./subdir` instead of `./parent_dir/subdir`. This causes `ENOENT` ("No such file or directory") errors for nested directories. Building full paths (`parent_dir/subdir`) ensures system calls resolve correct relative paths without altering global process working directory state via `chdir()`.
