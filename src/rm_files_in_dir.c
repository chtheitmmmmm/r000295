#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILENAME_LEN 32
#define NULL_RETURN(ptr, ret) if (!(ptr)) return ret;

// 获取一行，返回获取的长度
size_t get_line(char * restrict dest, size_t max_size) {
    int c;
    size_t size = 0;
    while ((c = getchar()) != '\n' && c != EOF && size < max_size) {
        dest[size++] = (char) c;
    }
    return size;
}

struct dirent *search_directory(DIR *src_dir, char *filter,
    size_t *created, char **delete_choices)
{
    size_t files_max = 10;
    size_t files_size = 0;
    struct dirent *files = (struct dirent*)malloc(sizeof(struct dirent) * files_max);
    NULL_RETURN(files, NULL);
    *delete_choices = (char*)calloc(files_max, sizeof(char));
    NULL_RETURN(*delete_choices, NULL);
    *created = 0;
    struct dirent *src_dirent;
    // Process each entry
    while (1) {
        src_dirent = readdir(src_dir);
        if (src_dirent == NULL)
            break;
        if (files_size >= files_max) {
            files_max *= 2;
            files = (struct dirent*)realloc(files, sizeof(struct dirent) * files_max);
            NULL_RETURN(files, NULL);
            *delete_choices = (char*)realloc(*delete_choices, files_max);
            NULL_RETURN(delete_choices, NULL);
        }
        files[files_size++] = *src_dirent;
        printf ("Visiting [%s]\n", src_dirent->d_name);
        // does this file match the filter?
        unsigned char offset = strlen(src_dirent->d_name) - strlen(filter);
        if (strncmp(src_dirent->d_name + offset, filter, strlen(filter)) == 0) {
            // ask for delete or not
            printf("Delete %s (y/n)", src_dirent->d_name);
            char delete_conf[1];
            if (get_line(delete_conf, 1) == 1) {
                if (toupper(delete_conf[0]) == 'Y') {
                    // copy the structure into heap memory
                    (*delete_choices)[files_size - 1] = 1;
                }
            }
        }
    }
    *created = files_size;
    return files;
}

// uses one of two methods to remove a file
// - C remove() function
// - shell fork + exec
// return 0 on success
int remove_file(char use_remove, char *cmd, char *filename, char *cwd, char * dir) {
    size_t buffer_size = strlen(filename) + strlen(cwd) + strlen(dir) + 2;
    char * buffer = (char*) calloc(buffer_size + 1, sizeof(char));
    sprintf(buffer, "%s/%s/%s", cwd, dir, filename);
    if (use_remove) {
        int ret = remove(buffer);
        free(buffer);
        return ret;
    } else {
        // fork and exec
        int cid = fork();
        if (cid == 0) {
            // child
            if (-1 == setenv("PWD", cwd, 1))
                return -1;
            // we use execvp because it will search PATH variable
            char *argv[3];
            argv[0] = cmd;
            argv[1] = buffer;
            argv[2] = NULL;
            if (-1 == execvp(cmd, argv)) {
                free(buffer);
                return -1;
            }
            free(buffer);
            return 0;
        } else {
            int status;
            waitpid(cid, &status, 0);
            free(buffer);
            return status;
        }
    }
}

int main (int argc, char **argv) {
    DIR *src_dir;
    // Ensure correct argument count
    if (argc != 2) {
        printf ("Usage: rm_files_in_dir <dir>\n");
        return 1;
    }
    char * path = getenv("PATH");
    if (path)
        printf("PATH : %s\n", getenv("PATH"));
    else
        printf("$PATH doesn't exists.\n");
    char * home = getenv("HOME");
    if (home)
        printf("HOME : %s\n", getenv("HOME"));
    else
        printf("$HOME doesn't exists.\n");
    printf("Enter filter: ");
    char filter[FILENAME_LEN] = {0};
    if (get_line(filter, FILENAME_LEN) == FILENAME_LEN) {
        printf("Filter too long, please make sure its length shorter then %d\n", FILENAME_LEN);
        return 4;
    }
    // Ensure we can open directory
    src_dir = opendir (argv[1]);
    if (src_dir == NULL) {
        printf ("Failed to open directory '%s'\n", argv[1]);
        return 2;
    }
    size_t created = 0;
    struct dirent *files;
    char *delete_choices;
    files = search_directory(src_dir, filter, &created,
                            &delete_choices);
    if (!files) {
         printf("Error occurred when globing files.\n");
         return 3;
    }
    closedir (src_dir);
    printf("You are going to delete the following: \n");
    size_t i = 0;
    for (; i < created; ++i) {
        if (delete_choices[i]) {
            printf("%s\n", files[i].d_name);
        }
    }
    printf("Do you want to proceed? Y/N ");
    // ask for delete or not
    char delete_conf[1];    // todo: 这里应该设置足够多的缓冲区
    if (get_line(delete_conf, 1) == 1 && toupper(delete_conf[0]) == 'Y') {
        for (i = 0; i < created; ++i) {
            if (delete_choices[i]) {
                // based on dirent
                char *current_filename = files[i].d_name;
                int ret = remove_file(1, "rm", current_filename, getenv("PWD"), argv[1]);
                if (ret == 0)
                    printf("OK     %s\n", current_filename);
                else
                    printf("FAILED %s\n", current_filename);
            }
        }
    } else {
        printf("Aborting.\n");
    }
    free(files);
    free(delete_choices);
    return 0;
}
