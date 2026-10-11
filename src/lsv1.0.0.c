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

void do_ls(const char *dir, int mode);
void show_file_info(const char *filename, const struct stat *info_p);
void mode_to_letters(int mode, char str[]);
void print_columnar(char **filenames, int count, int max_len);
void print_horizontal(char **filenames, int count, int max_len);

int main(int argc, char *argv[])
{
    int mode = MODE_DEFAULT;
    int opt;

    // Task 2: Extend getopt() parsing to recognize -x flag
    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
            case 'l':
                mode = MODE_LONG;
                break;
            case 'x':
                mode = MODE_HORIZONTAL;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        do_ls(".", mode);
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1) {
                printf("%s:\n", argv[i]);
            }
            do_ls(argv[i], mode);
            if (i < argc - 1) {
                puts("");
            }
        }
    }
    return 0;
}

void do_ls(const char *dir, int mode)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    // Task 4: Integrate display modes
    if (mode == MODE_LONG) {
        errno = 0;
        while ((entry = readdir(dp)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;

            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

            struct stat info;
            if (lstat(path, &info) == -1) {
                perror("lstat failed");
                continue;
            }
            show_file_info(entry->d_name, &info);
        }
        if (errno != 0) {
            perror("readdir failed");
        }
    } else {
        // Read entries into dynamic array
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

        // Route to requested display function
        if (count > 0) {
            if (mode == MODE_HORIZONTAL) {
                print_horizontal(filenames, count, max_len);
            } else {
                print_columnar(filenames, count, max_len);
            }
        }

        for (int i = 0; i < count; i++) {
            free(filenames[i]);
        }
        free(filenames);
    }

    closedir(dp);
}

// Task 3: Horizontal Display Logic (Across)
void print_horizontal(char **filenames, int count, int max_len)
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
        // Print file entry padded to column width
        if (current_col == num_cols - 1 || i == count - 1) {
            printf("%s\n", filenames[i]);
            current_col = 0;
        } else {
            printf("%-*s", col_width, filenames[i]);
            current_col++;
        }
    }

    if (current_col != 0) {
        printf("\n");
    }
}

// Default Columnar Display Logic (Down Then Across)
void print_columnar(char **filenames, int count, int max_len)
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
                    printf("%s", filenames[idx]);
                } else {
                    printf("%-*s", col_width, filenames[idx]);
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

void show_file_info(const char *filename, const struct stat *info_p)
{
    char modestr[11];
    mode_to_letters(info_p->st_mode, modestr);

    struct passwd *pw = getpwuid(info_p->st_uid);
    char *user_name = pw ? pw->pw_name : "UNKNOWN";

    struct group *grp = getgrgid(info_p->st_gid);
    char *group_name = grp ? grp->gr_name : "UNKNOWN";

    char datestr[100];
    strncpy(datestr, ctime(&info_p->st_mtime) + 4, 12);
    datestr[12] = '\0';

    printf("%s %2ld %-8s %-8s %8ld %s %s\n",
           modestr,
           (long)info_p->st_nlink,
           user_name,
           group_name,
           (long)info_p->st_size,
           datestr,
           filename);
}
