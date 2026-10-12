#define _DEFAULT_SOURCE
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
#include <sys/ioctl.h>

extern int errno;

// Display modes state management
#define MODE_DEFAULT    0
#define MODE_LONG       1
#define MODE_HORIZONTAL 2

// ANSI Escape Codes for File Types
#define COLOR_RESET   "\033[0m"
#define COLOR_DIR     "\033[1;34m" // Blue (Bold)
#define COLOR_EXEC    "\033[1;32m" // Green (Bold)
#define COLOR_ARCHIVE "\033[1;31m" // Red (Bold)
#define COLOR_SYMLINK "\033[1;35m" // Pink / Magenta (Bold)
#define COLOR_SPECIAL "\033[7m"    // Reverse Video

void do_ls(const char *dir, int mode, int recursive, int is_first_dir);
void show_file_info(const char *dir, const char *filename);
void mode_to_letters(int mode, char str[]);
void print_columnar(const char *dir, char **filenames, int count, int max_len);
void print_horizontal(const char *dir, char **filenames, int count, int max_len);
int compare_strings(const void *a, const void *b);
void print_colored_filename(const char *dir, const char *filename, int padding);

int main(int argc, char *argv[])
{
    int mode = MODE_DEFAULT;
    int recursive = 0;
    int opt;

    // Task 2: Extend getopt() for -R flag
    while ((opt = getopt(argc, argv, "lxR")) != -1) {
        switch (opt) {
            case 'l':
                mode = MODE_LONG;
                break;
            case 'x':
                mode = MODE_HORIZONTAL;
                break;
            case 'R':
                recursive = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        do_ls(".", mode, recursive, 1);
    } else {
        for (int i = optind; i < argc; i++) {
            int is_first = (i == optind) ? 1 : 0;
            if (!is_first) {
                puts("");
            }
            do_ls(argv[i], mode, recursive, is_first);
        }
    }
    return 0;
}

int compare_strings(const void *a, const void *b)
{
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;
    return strcmp(str1, str2);
}

void print_colored_filename(const char *dir, const char *filename, int padding)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    struct stat info;
    if (lstat(path, &info) == -1) {
        if (padding > 0)
            printf("%-*s", padding, filename);
        else
            printf("%s", filename);
        return;
    }

    const char *color = COLOR_RESET;

    int len = strlen(filename);
    int is_archive = 0;
    if ((len > 4 && strcmp(filename + len - 4, ".tar") == 0) ||
        (len > 3 && strcmp(filename + len - 3, ".gz") == 0)  ||
        (len > 4 && strcmp(filename + len - 4, ".zip") == 0) ||
        (len > 4 && strcmp(filename + len - 4, ".tgz") == 0)) {
        is_archive = 1;
    }

    if (S_ISLNK(info.st_mode)) {
        color = COLOR_SYMLINK;
    } else if (S_ISDIR(info.st_mode)) {
        color = COLOR_DIR;
    } else if (is_archive) {
        color = COLOR_ARCHIVE;
    } else if (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        color = COLOR_EXEC;
    } else if (S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode) ||
               S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)) {
        color = COLOR_SPECIAL;
    }

    if (padding > 0) {
        int spaces = padding - (int)strlen(filename);
        if (spaces < 0) spaces = 0;
        printf("%s%s%s%*s", color, filename, COLOR_RESET, spaces, "");
    } else {
        printf("%s%s%s", color, filename, COLOR_RESET);
    }
}

// Task 3: Recursive do_ls implementation
void do_ls(const char *dir, int mode, int recursive, int is_first_dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    // Print header if recursive or multi-directory
    if (recursive || !is_first_dir) {
        printf("%s:\n", dir);
    }

    int capacity = 16;
    int count = 0;
    int max_len = 0;
    char **filenames = malloc(capacity * sizeof(char *));

    if (!filenames) {
        perror("malloc failed");
        closedir(dp);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(filenames, capacity * sizeof(char *));
            if (!tmp) {
                perror("realloc failed");
                break;
            }
            filenames = tmp;
        }

        filenames[count] = strdup(entry->d_name);
        int len = strlen(entry->d_name);
        if (len > max_len) {
            max_len = len;
        }
        count++;
    }

    if (errno != 0) {
        perror("readdir failed");
    }
    closedir(dp);

    if (count > 0) {
        qsort(filenames, count, sizeof(char *), compare_strings);

        // Render current directory contents
        if (mode == MODE_LONG) {
            for (int i = 0; i < count; i++) {
                show_file_info(dir, filenames[i]);
            }
        } else if (mode == MODE_HORIZONTAL) {
            print_horizontal(dir, filenames, count, max_len);
        } else {
            print_columnar(dir, filenames, count, max_len);
        }

        // Phase 2: Handle recursion (-R)
        if (recursive) {
            for (int i = 0; i < count; i++) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", dir, filenames[i]);

                struct stat info;
                if (lstat(path, &info) == -1) {
                    continue;
                }

                // Recurse into directory if not symlink, . or ..
                if (S_ISDIR(info.st_mode) && !S_ISLNK(info.st_mode)) {
                    puts("");
                    do_ls(path, mode, recursive, 0);
                }
            }
        }
    }

    for (int i = 0; i < count; i++) {
        free(filenames[i]);
    }
    free(filenames);
}

void print_horizontal(const char *dir, char **filenames, int count, int max_len)
{
    struct winsize w;
    int term_width = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        term_width = w.ws_col;
    }

    int spacing = 2;
    int col_width = max_len + spacing;
    int num_cols = term_width / col_width;

    if (num_cols < 1) {
        num_cols = 1;
    }

    int current_col = 0;
    for (int i = 0; i < count; i++) {
        if (current_col == num_cols - 1 || i == count - 1) {
            print_colored_filename(dir, filenames[i], 0);
            printf("\n");
            current_col = 0;
        } else {
            print_colored_filename(dir, filenames[i], col_width);
            current_col++;
        }
    }

    if (current_col != 0) {
        printf("\n");
    }
}

void print_columnar(const char *dir, char **filenames, int count, int max_len)
{
    struct winsize w;
    int term_width = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        term_width = w.ws_col;
    }

    int spacing = 2;
    int col_width = max_len + spacing;
    int num_cols = term_width / col_width;

    if (num_cols < 1) {
        num_cols = 1;
    }

    int num_rows = (count + num_cols - 1) / num_cols;

    for (int r = 0; r < num_rows; r++) {
        for (int c = 0; c < num_cols; c++) {
            int idx = r + c * num_rows;
            if (idx < count) {
                if (c == num_cols - 1 || idx + num_rows >= count) {
                    print_colored_filename(dir, filenames[idx], 0);
                } else {
                    print_colored_filename(dir, filenames[idx], col_width);
                }
            }
        }
        printf("\n");
    }
}

void mode_to_letters(int mode, char str[])
{
    strcpy(str, "----------");

    if (S_ISDIR(mode))  str[0] = 'd';
    if (S_ISCHR(mode))  str[0] = 'c';
    if (S_ISBLK(mode))  str[0] = 'b';
    if (S_ISLNK(mode))  str[0] = 'l';
    if (S_ISFIFO(mode)) str[0] = 'p';
    if (S_ISSOCK(mode)) str[0] = 's';

    if (mode & S_IRUSR) str[1] = 'r';
    if (mode & S_IWUSR) str[2] = 'w';
    if (mode & S_IXUSR) str[3] = 'x';

    if (mode & S_IRGRP) str[4] = 'r';
    if (mode & S_IWGRP) str[5] = 'w';
    if (mode & S_IXGRP) str[6] = 'x';

    if (mode & S_IROTH) str[7] = 'r';
    if (mode & S_IWOTH) str[8] = 'w';
    if (mode & S_IXOTH) str[9] = 'x';

    if (mode & S_ISUID) str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (mode & S_IXOTH) ? 't' : 'T';
}

void show_file_info(const char *dir, const char *filename)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    struct stat info;
    if (lstat(path, &info) == -1) {
        perror("lstat failed");
        return;
    }

    char modestr[11];
    mode_to_letters(info.st_mode, modestr);

    struct passwd *pw = getpwuid(info.st_uid);
    char *user_name = pw ? pw->pw_name : "UNKNOWN";

    struct group *grp = getgrgid(info.st_gid);
    char *group_name = grp ? grp->gr_name : "UNKNOWN";

    char datestr[100];
    strncpy(datestr, ctime(&info.st_mtime) + 4, 12);
    datestr[12] = '\0';

    printf("%s %2ld %-8s %-8s %8ld %s ",
           modestr,
           (long)info.st_nlink,
           user_name,
           group_name,
           (long)info.st_size,
           datestr);

    print_colored_filename(dir, filename, 0);
    printf("\n");
}
