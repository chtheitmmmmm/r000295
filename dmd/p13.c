#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct dirent *search_directory(DIR *src_dir, char *filter, 
    size_t *created, char **delete_choices, size_t *to_delete)
{
    size_t files_max = 10;
    size_t files_size = 0;
    struct dirent *files = (struct dirent*)malloc(sizeof(struct dirent) * files_max);
    *delete_choices = (char*)malloc(files_max);
    *created = 0;
    *to_delete = 0;

    struct dirent *src_dirent;

    // Process each entry
    while (1)
    {

        if (files_size >= files_max) 
        {
            files_max *= 2;
            files = (struct dirent*)realloc(files, sizeof(struct dirent) * files_max);
            *delete_choices = (char*)realloc(delete_choices, files_max);
        }

        src_dirent = readdir(src_dir);
        if (src_dirent == NULL)
            break;

        printf ("Visiting [%s]\n", src_dirent->d_name);

        // copy the structure into heap memory
        memcpy(files+files_size, src_dirent, sizeof(struct dirent));

        // based on dirent
        char current_filename[32];
        strcpy(current_filename, src_dirent->d_name);

        // does this file match the filter?
        unsigned char offset = strlen(current_filename) - strlen(filter);

        if (strncmp(current_filename + offset, filter, strlen(filter)) == 0)
        {
            // ask for delete or not
            printf("Delete %s (y/n)", current_filename);
            char delete_conf[1];
            gets(delete_conf);

            if (toupper(delete_conf[0]) == 'Y')
            {
                *to_delete = *to_delete + 1;
                *delete_choices[ *to_delete ] = 1;
            }

            printf("\n");

        }

    }

    *created = files_size;
    return files;
}

// uses one of two methods to remove a file
// - C remove() function
// - shell fork + exec
// return 0 on success
int remove_file(char use_remove, char *cmd, char *filename, char *cwd)
{
    if (use_remove)
    {
        return remove(filename);
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
            argv[1] = filename;
            argv[2] = NULL;
            if (-1 == execvp(cmd, argv))
                return -1;

            return 0;
        } else {
            int status;
            waitpid(cid, &status, 0);
            return status;            
        }
    }
}

int main (int argc, char **argv) 
{
    DIR *src_dir;

    // Ensure correct argument count
    if (argc != 2) 
    {
        printf ("Usage: rm_files_in_dir <dir>\n");
        return 1;
    }

    // Check environment variables
    char home[10000];
    char path[10000];
    strcpy(path, getenv("PATH"));
    strcpy(home, getenv("HOME"));

    printf("PATH : %s\n", getenv("PATH"));
    printf("HOME : %s\n", getenv("HOME"));

    printf("Enter filter: ");
    char filter[32];
    gets(filter);

    // Ensure we can open directory
    src_dir = opendir (argv[1]);
    if (src_dir == NULL) 
    {
        printf ("Failed to open directory '%s'\n", argv[1]);
        return 2;
    }

    size_t created = 0;
    size_t to_delete = 0;
    struct dirent *files;
    char *delete_choices;

    files = search_directory(src_dir, filter, &created, 
                            &delete_choices, &to_delete);
    closedir (src_dir);

    printf("You are going to delete the following: \n");

    size_t i = 0;
    for (; i < created; ++i) 
    {
        printf("%s\n", files[i].d_name);
    }

    printf("Do you want to proceed? Y/N ");

    // ask for delete or not
    char delete_conf[1];
    gets(delete_conf);
    if (toupper(delete_conf[0]) == 'Y')
    {
        for (; i < to_delete; ++i)
        {
            // based on dirent
            char *current_filename = files[i].d_name;

            if (0 != delete_choices[i]) 
            {
                int ret = remove_file(1, "rm", current_filename, getenv("PWD"));
                if (ret == 0)
                    printf("OK     %s", current_filename);
                else
                    printf("FAILED %s", current_filename);
            }
        }
    } else {
        printf("Aborting.\n");
    }

    free(files);
    free(delete_choices);

    return 0;
}
