/*
 * Programming Assignment 02: lsv1.0.0
 * A custom implementation of the ls utility.
 *
 * Features implemented:
 *   - Default: multi-column "down then across" display
 *   - -l      : long listing format
 *   - -x      : horizontal "across" display
 *   - Sorting: alphabetical
 *   - Colors : by file type (directory, executable, archive, symlink, special)
 *
 * Usage:
 *   $ ./bin/ls
 *   $ ./bin/ls -l
 *   $ ./bin/ls -x
 *   $ ./bin/ls /home
 *   $ ./bin/ls -l /etc
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

/* ---- ANSI color escape codes ---- */
#define COL_RESET   "\033[0m"
#define COL_BLUE    "\033[0;34m"
#define COL_GREEN   "\033[0;32m"
#define COL_RED     "\033[0;31m"
#define COL_PINK    "\033[1;35m"
#define COL_REVERSE "\033[7m"

extern int errno;

/* ---- Function prototypes ---- */
void do_ls(const char *dir, int long_format, int horizontal_format);
void print_permissions(mode_t mode);
void print_long(const char *dirname, const char *filename);
int  get_terminal_width(void);
void print_columns(const char *dirname, char **files, int count, int max_len);
void print_horizontal(const char *dirname, char **files, int count, int max_len);
int  compare_strings(const void *a, const void *b);
void print_colored(const char *dirname, const char *filename);

/* ==================================================================
 * print_permissions: builds a 10-char string like "-rwxr-xr-x"
 * from the numeric st_mode.
 * ================================================================== */
void print_permissions(mode_t mode)
{
    char perms[11] = "----------";

    if (S_ISDIR(mode))       perms[0] = 'd';
    else if (S_ISLNK(mode))  perms[0] = 'l';
    else if (S_ISCHR(mode))  perms[0] = 'c';
    else if (S_ISBLK(mode))  perms[0] = 'b';
    else if (S_ISFIFO(mode)) perms[0] = 'p';
    else if (S_ISSOCK(mode)) perms[0] = 's';
    else                     perms[0] = '-';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';

    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    printf("%s", perms);
}

/* ==================================================================
 * print_long: prints one file in long-listing format.
 * ================================================================== */
void print_long(const char *dirname, const char *filename)
{
    struct stat buf;
    char fullpath[2048];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, filename);

    if (lstat(fullpath, &buf) == -1) {
        perror("lstat");
        return;
    }

    print_permissions(buf.st_mode);
    printf(" %2ld", buf.st_nlink);

    struct passwd *pw = getpwuid(buf.st_uid);
    printf(" %s", pw ? pw->pw_name : "?");

    struct group *gr = getgrgid(buf.st_gid);
    printf(" %s", gr ? gr->gr_name : "?");

    printf(" %8ld", buf.st_size);

    char *timestr = ctime(&buf.st_mtime);
    timestr[16] = '\0';
    printf(" %s", timestr + 4);

    printf(" %s\n", filename);
}

/* ==================================================================
 * get_terminal_width: returns the number of columns of the terminal.
 * Falls back to 80 if ioctl fails.
 * ================================================================== */
int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
        return 80;
    return w.ws_col;
}

/* ==================================================================
 * compare_strings: comparison function for qsort (sorts strings).
 * ================================================================== */
int compare_strings(const void *a, const void *b)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

/* ==================================================================
 * print_colored: prints a filename with a color based on its type.
 * ================================================================== */
void print_colored(const char *dirname, const char *filename)
{
    struct stat buf;
    char fullpath[2048];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, filename);

    if (lstat(fullpath, &buf) == -1) {
        printf("%s", filename);
        return;
    }

    if (S_ISLNK(buf.st_mode)) {
        printf(COL_PINK "%s" COL_RESET, filename);
        return;
    }
    if (S_ISDIR(buf.st_mode)) {
        printf(COL_BLUE "%s" COL_RESET, filename);
        return;
    }
    if (S_ISCHR(buf.st_mode) || S_ISBLK(buf.st_mode) ||
        S_ISFIFO(buf.st_mode) || S_ISSOCK(buf.st_mode)) {
        printf(COL_REVERSE "%s" COL_RESET, filename);
        return;
    }
    if (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        printf(COL_GREEN "%s" COL_RESET, filename);
        return;
    }

    const char *ext = strrchr(filename, '.');
    if (ext && (strcmp(ext, ".tar") == 0 ||
                strcmp(ext, ".gz")  == 0 ||
                strcmp(ext, ".zip") == 0)) {
        printf(COL_RED "%s" COL_RESET, filename);
        return;
    }

    printf("%s", filename);
}

/* ==================================================================
 * print_columns: prints files in "down then across" columns.
 * ================================================================== */
void print_columns(const char *dirname, char **files, int count, int max_len)
{
    int term_width = get_terminal_width();
    int col_width = max_len + 2;
    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    if (num_cols > count) num_cols = count;

    int num_rows = (count + num_cols - 1) / num_cols;

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int index = col * num_rows + row;
            if (index >= count) continue;

            int name_len = strlen(files[index]);
            print_colored(dirname, files[index]);
            for (int k = name_len; k < col_width; k++)
                printf(" ");
        }
        printf("\n");
    }
}

/* ==================================================================
 * print_horizontal: prints files in "across" columns.
 * ================================================================== */
void print_horizontal(const char *dirname, char **files, int count, int max_len)
{
    int term_width = get_terminal_width();
    int col_width = max_len + 2;

    int pos = 0;
    for (int i = 0; i < count; i++) {
        if (pos + col_width > term_width) {
            printf("\n");
            pos = 0;
        }
        int name_len = strlen(files[i]);
        print_colored(dirname, files[i]);
        for (int k = name_len; k < col_width; k++)
            printf(" ");
        pos += col_width;
    }
    if (pos > 0) printf("\n");
}

/* ==================================================================
 * do_ls: reads a directory, sorts it, and displays it.
 * ================================================================== */
void do_ls(const char *dir, int long_format, int horizontal_format)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **files = NULL;
    int count = 0;
    int capacity = 0;
    int max_len = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            files = realloc(files, capacity * sizeof(char *));
            if (files == NULL) {
                perror("realloc");
                closedir(dp);
                return;
            }
        }

        files[count] = strdup(entry->d_name);
        count++;

        int len = strlen(entry->d_name);
        if (len > max_len) max_len = len;
    }

    if (errno != 0)
        perror("readdir failed");

    qsort(files, count, sizeof(char *), compare_strings);

    if (long_format) {
        for (int i = 0; i < count; i++)
            print_long(dir, files[i]);
    } else if (horizontal_format) {
        print_horizontal(dir, files, count, max_len);
    } else {
        print_columns(dir, files, count, max_len);
    }

    for (int i = 0; i < count; i++)
        free(files[i]);
    free(files);

    closedir(dp);
}

/* ==================================================================
 * main: parses -l and -x flags using getopt.
 * ================================================================== */
int main(int argc, char *const argv[])
{
    int opt;
    int long_format = 0;
    int horizontal_format = 0;

    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
            case 'l':
                long_format = 1;
                break;
            case 'x':
                horizontal_format = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [dir...]\n", argv[0]);
                exit(1);
        }
    }

    if (optind == argc) {
        do_ls(".", long_format, horizontal_format);
    } else {
        for (int i = optind; i < argc; i++) {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_format, horizontal_format);
            puts("");
        }
    }

    return 0;
}
