# PA-02 Report: Custom ls Implementation

**Student:** Hifza Zia
**Roll No:** BSDSF24M034
**Course:** Operating Systems
**Repository:** BSDSF24M034-OS-A02

---

## Part 2: Long Listing (-l)

### Q1: Difference between stat() and lstat()

Both stat() and lstat() retrieve file metadata. The difference is how they handle symbolic links:
- stat() follows symlinks and returns info about the target file.
- lstat() does NOT follow symlinks and returns info about the link itself.

We use lstat() in ls -l because we want to show the symlink's own permissions (lrwxrwxrwx) and its target, not the target's permissions.

### Q2: How st_mode encodes file type and permissions

st_mode is a 16-bit integer. The upper bits (12-15) encode the file type (regular, directory, symlink). The lower 9 bits encode permissions for owner, group, and others (3 bits each).

To extract the type, we use macros like S_ISDIR(), S_ISLNK(), which internally do:
  (mode & S_IFMT) == S_IFDIR

To extract permissions, we use bitwise AND:
  if (mode & S_IRUSR) perms[1] = 'r';
  if (mode & S_IWUSR) perms[2] = 'w';
  if (mode & S_IXUSR) perms[3] = 'x';

Each permission bit represents whether that permission is granted.

---

## Part 3: Column Display

### Q1: Why a single loop is insufficient for down-then-across

Down-then-across fills column by column, not row by row. If we used a single loop, we would print item[0], item[1], item[2], which is "across then down". To achieve "down then across", we need nested loops. The outer loop iterates over rows, and the inner loop iterates over columns. The index is calculated as:
  index = col * num_rows + row

This ensures items fill vertically within each column.

### Q2: Purpose of ioctl(TIOCGWINSZ) and limitations

ioctl(TIOCGWINSZ) queries the terminal for its dimensions (width and height in characters). This lets ls adapt to the user's terminal width.

Limitations:
1. Not all terminals support it (e.g., when output is redirected to a file).
2. We need a fallback (80 columns) when it fails.
3. It doesn't account for wide characters or tabs.
4. Terminal resizing after start is not detected.

We default to 80 columns if ioctl returns -1.

---

## Part 4: Horizontal Display (-x)

### Q1: Complexity comparison

Down-then-across requires:
- Nested loops with index formula col * num_rows + row
- Pre-computing num_rows and num_cols
- Time: O(n), Space: O(n)

Horizontal (-x) requires:
- Single loop with position counter
- When pos + col_width > term_width, print newline and reset
- Time: O(n), Space: O(n)

Horizontal is simpler logically because it avoids computing rows and columns in advance.

### Q2: State management strategy

We use integer flags in main() for each mode:
  int long_format = 0;
  int horizontal_format = 0;
  int recursive = 0;

getopt() sets these flags when options are passed:
  case 'l': long_format = 1; break;
  case 'x': horizontal_format = 1; break;
  case 'R': recursive = 1; break;

These flags are passed to do_ls(), which decides which print function to call.

---

## Part 5: Alphabetical Sort

### Q1: Why read all entries before sorting

To sort entries, all of them must be available simultaneously. readdir() returns entries in filesystem order, not sorted. We must read all into an array, then qsort(), then print.

Drawbacks:
1. Memory usage for huge directories.
2. Two passes needed (read, then print).
3. No streaming output.
4. realloc() overhead when growing the array.

### Q2: qsort compare function with const void *

Our function:
  int compare_strings(const void *a, const void *b) {
      const char *sa = *(const char **)a;
      const char *sb = *(const char **)b;
      return strcmp(sa, sb);
  }

qsort is generic and uses void * to sort any type. Our array is char *files[], so each element is char *. Therefore a and b are char ** pointers.

We cast: (const char **)a gives a pointer to char *, and *(const char **)a dereferences it to get the actual string.

const means the function must not modify the elements.

Return value: negative if a < b, zero if equal, positive if a > b. strcmp already provides this.

---

## Part 6: Colorized Output

### Q1: ANSI escape codes

ANSI escape codes are special character sequences that change terminal formatting. They start with \033 followed by [ and a code, ending with m.

Example for green:
  #define COL_GREEN "\033[0;32m"
  #define COL_RESET "\033[0m"
  printf(COL_GREEN "%s" COL_RESET, filename);

Output: filename printed in green, then reset.

Always reset after coloring, otherwise all subsequent output stays colored.

### Q2: Which bits determine executability

Three bits in st_mode:
- S_IXUSR (00100) - owner execute
- S_IXGRP (00010) - group execute
- S_IXOTH (00001) - others execute

We check: if (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
If any is set, the file is executable and printed green.

---

## Part 7: Recursive Listing (-R)

### Q1: Base case in recursion

The base case is when a directory contains no subdirectories. When do_ls() is called on such a directory, after printing entries, the recursive loop finds no directory entries and returns without any recursive call.

Termination is guaranteed because:
1. Filesystems are finite.
2. We skip . and .. (which would cause infinite loops).
3. Each recursive call goes one level deeper, bounded by max depth.

### Q2: Why construct full path before recursing

When we recurse, we need a valid path to the subdirectory. The name alone (e.g., "bin") isn't enough because opendir() interprets paths relative to the current working directory, not the parent we're iterating.

We build:
  char fullpath[2048];
  snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, files[i]);
  do_ls(fullpath, ...);

snprintf is used instead of strcpy/strcat because it prevents buffer overflows by limiting the number of characters written.

Without this full path, recursion would fail or open the wrong directory.

---

## Summary

All 12 report questions are answered above, covering:
- stat vs lstat, st_mode bit manipulation
- Column display logic, ioctl terminal width
- Horizontal vs. vertical complexity, state management
- Sorting with qsort, const void * pointers
- ANSI color codes, executability bits
- Recursion base case, full path construction
