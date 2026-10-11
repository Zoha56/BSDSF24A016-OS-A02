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

extern int errno;

void do_ls(const char *dir, int long_format);
void show_file_info(const char *filename, const struct stat *info_p);
void mode_to_letters(int mode, char str[]);

int main(int argc, char *argv[])
{
    int flag_l = 0;
    int opt;

    while ((opt = getopt(argc, argv, "l")) != -1) {
        switch (opt) {
            case 'l':
                flag_l = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        do_ls(".", flag_l);
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1) {
                printf("%s:\n", argv[i]);
            }
            do_ls(argv[i], flag_l);
            if (i < argc - 1) {
                puts("");
            }
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_format)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (long_format) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

            struct stat info;
            if (lstat(path, &info) == -1) {
                perror("lstat failed");
                continue;
            }
            show_file_info(entry->d_name, &info);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    if (errno != 0) {
        perror("readdir failed");
    }

    closedir(dp);
}

void mode_to_letters(int mode, char str[])
{
    strcpy(str, "----------");

    // File type
    if (S_ISDIR(mode))  str[0] = 'd';
    if (S_ISCHR(mode))  str[0] = 'c';
    if (S_ISBLK(mode))  str[0] = 'b';
    if (S_ISLNK(mode))  str[0] = 'l';
    if (S_ISFIFO(mode)) str[0] = 'p';
    if (S_ISSOCK(mode)) str[0] = 's';

    // Permissions
    if (mode & S_IRUSR) str[1] = 'r';
    if (mode & S_IWUSR) str[2] = 'w';
    if (mode & S_IXUSR) str[3] = 'x';

    if (mode & S_IRGRP) str[4] = 'r';
    if (mode & S_IWGRP) str[5] = 'w';
    if (mode & S_IXGRP) str[6] = 'x';

    if (mode & S_IROTH) str[7] = 'r';
    if (mode & S_IWOTH) str[8] = 'w';
    if (mode & S_IXOTH) str[9] = 'x';

    // Special bits
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
