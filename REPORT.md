X# REPORT.md — Operating Systems Assignment 2

## Part 2: Complete Long Listing Format (-l)

### Q1. Difference between `stat()` and `lstat()`

The `stat()` and `lstat()` system calls both retrieve file metadata from the inode into a `struct stat`. The crucial difference is how they handle **symbolic links**:

- **`stat()`** follows symbolic links. If the path points to a symlink, `stat()` returns the metadata of the **target file** (the file the symlink points to).
- **`lstat()`** does NOT follow symbolic links. If the path points to a symlink, `lstat()` returns the metadata of the **symlink itself**.

**When to use `lstat()` in `ls`:**
The `ls -l` command must display the properties of the symlink itself (its own permissions, size, and name). If `ls -l` used `stat()`, a symlink would appear identical to its target, hiding the fact that it is a link. Therefore, `lstat()` is more appropriate when listing directory entries that may be symbolic links, so that the symlink is displayed as a distinct entity.

---

### Q2. Extracting file type and permissions from `st_mode`

The `st_mode` field is a 16-bit integer. Its lower 12 bits encode the file type (bits 15–12) and the permission bits (bits 11–0). We use **bitwise AND (`&`)** to isolate specific bits, and predefined macros from `<sys/stat.h>` to compare the result.

**Extracting the file type:**
The file type is stored in the top 4 bits (bits 15–12). The mask `S_IFMT` (0170000 in octal) covers exactly those bits. To get the file type:
```c
mode_t type = buf.st_mode & S_IFMT;
