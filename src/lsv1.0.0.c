/*
 * Programming Assignment 02: lsv1.0.0
 * Base ls implementation with -l long listing support.
 * Usage:
 *     $ ./bin/ls
 *     $ ./bin/ls -l
 *     $ ./bin/ls -l /home
 *     $ ./bin/ls -l /home /etc
 */
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

/* ---- Prototypes ---- */
int compare_strings(const void *a, const void *b);
void do_ls(const char *dir, int long_format, int horizontal_format);
void print_permissions(mode_t mode);
void print_long(const char *dirname, const char *filename);
int get_terminal_width(void);
void print_columns(char **files, int count, int max_len);

/* ---- print_permissions: prints rwx string from st_mode ---- */
void print_permissions(mode_t mode)
{
    char perms[11] = "----------";

    /* File type */
    if (S_ISDIR(mode))       perms[0] = 'd';
    else if (S_ISLNK(mode))  perms[0] = 'l';
    else if (S_ISCHR(mode))  perms[0] = 'c';
    else if (S_ISBLK(mode))  perms[0] = 'b';
    else if (S_ISFIFO(mode)) perms[0] = 'p';
    else if (S_ISSOCK(mode)) perms[0] = 's';
    else                     perms[0] = '-';

    /* Owner */
    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';

    /* Group */
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    /* Others */
    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    printf("%s", perms);
}

/* ---- print_long: prints one file in long format ---- */
void print_long(const char *dirname, const char *filename)
{
    struct stat buf;
    char fullpath[2048];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, filename);

    if (lstat(fullpath, &buf) == -1) {
        perror("lstat");
        return;
    }

    /* 1. Permissions */
    print_permissions(buf.st_mode);

    /* 2. Link count */
    printf(" %2ld", buf.st_nlink);

    /* 3. Owner name */
    struct passwd *pw = getpwuid(buf.st_uid);
    printf(" %s", pw ? pw->pw_name : "?");

    /* 4. Group name */
    struct group *gr = getgrgid(buf.st_gid);
    printf(" %s", gr ? gr->gr_name : "?");

    /* 5. File size */
    printf(" %8ld", buf.st_size);

    /* 6. Time (last modified) */
    char *timestr = ctime(&buf.st_mtime);
    timestr[16] = '\0';        /* cut seconds + year */
    printf(" %s", timestr + 4); /* skip "Wed " */

    /* 7. Filename */
    printf(" %s\n", filename);
}
void print_columns(char **files, int count, int max_len)
{
    int term_width = get_terminal_width();
    int col_width = max_len + 2;   // padding between columns
    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    if (num_cols > count) num_cols = count;

    int num_rows = (count + num_cols - 1) / num_cols;

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int index = col * num_rows + row;
            if (index >= count) continue;

            printf("%-*s", col_width, files[index]);
        }
        printf("\n");
    }
}
void print_horizontal(char **files, int count, int max_len)
{
    int term_width = get_terminal_width();
    int col_width = max_len + 2;

    int pos = 0;
    for (int i = 0; i < count; i++) {
        if (pos + col_width > term_width) {
            printf("\n");
            pos = 0;
        }
        printf("%-*s", col_width, files[i]);
        pos += col_width;
    }
    if (pos > 0) printf("\n");
}
int compare_strings(const void *a, const void *b)
{
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strcmp(str_a, str_b);
}

int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
        return 80;   // fallback if ioctl fails
    return w.ws_col;
}

/* ---- main ---- */
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
/* ---- do_ls ---- */
void do_ls(const char *dir, int long_format, int horizontal_format)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    // Dynamic array of filenames
    char **files = NULL;
    int count = 0;
    int capacity = 0;
    int max_len = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        // Grow the array if needed
        if (count == capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            files = realloc(files, capacity * sizeof(char *));
            if (files == NULL) {
                perror("realloc");
                closedir(dp);
                return;
            }
        }

        // Duplicate the filename
        files[count] = strdup(entry->d_name);
        count++;

        // Track longest filename
        int len = strlen(entry->d_name);
        if (len > max_len) max_len = len;
    }

    if (errno != 0)
        perror("readdir failed");
    // Sort alphabetically
    qsort(files, count, sizeof(char *), compare_strings);

    // Display: long listing OR columns
    if (long_format) {
    for (int i = 0; i < count; i++) {
        print_long(dir, files[i]);
    }
} else if (horizontal_format) {
    print_horizontal(files, count, max_len);
} else {
    print_columns(files, count, max_len);
}

    // Free memory
    for (int i = 0; i < count; i++)
        free(files[i]);
    free(files);

    closedir(dp);
}
