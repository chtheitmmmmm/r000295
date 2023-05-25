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
    struct dirent *files = (struct dirent*)malloc(sizeof(struct dirent) * files_max);   // todo: 应检查是否分配成功
    *delete_choices = (char*)malloc(files_max);                                         // todo: 应检查是否分配成功 // todo: 用 calloc，因为我们要把它初始化为 0
    *created = 0;
    *to_delete = 0;

    struct dirent *src_dirent;

    // Process each entry
    while (1)
    {

        if (files_size >= files_max)  // todo: 此部分分配代码移动到下方更好，避免无用的内存分配
        {
            files_max *= 2;
            files = (struct dirent*)realloc(files, sizeof(struct dirent) * files_max);  // todo: 应该检查是否分配成功
            *delete_choices = (char*)realloc(delete_choices, files_max);                // todo: 应该检查是否分配成功
        }

        src_dirent = readdir(src_dir);
        if (src_dirent == NULL)
            break;

        printf ("Visiting [%s]\n", src_dirent->d_name);

        // copy the structure into heap memory
        memcpy(files+files_size, src_dirent, sizeof(struct dirent));    // todo: 此处代码可以简化，避免函数调用

        // todo: 此处的 current_filename 多余，直接用 d_name 就可
        // based on dirent
        char current_filename[32];
        strcpy(current_filename, src_dirent->d_name);   // todo: 此处应该检查缓冲区溢出

        // does this file match the filter?
        unsigned char offset = strlen(current_filename) - strlen(filter);

        if (strncmp(current_filename + offset, filter, strlen(filter)) == 0)
        {
            // ask for delete or not
            printf("Delete %s (y/n)", current_filename);
            char delete_conf[1];    // todo: 这里缓冲区大小不够，如果要读取一整行的话，
            gets(delete_conf);      // todo: 不要用 gets，不会检查缓冲区溢出

            if (toupper(delete_conf[0]) == 'Y')
            {
                *to_delete = *to_delete + 1;    // todo: 此处代码可以简化
                *delete_choices[ *to_delete ] = 1;  // todo: 此行代码应该和上一行代码交换位置。否则会逻辑错误
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
int remove_file(char use_remove, char *cmd, char *filename, char *cwd)  // todo: 应给出用户传入的目录 dirname
// todo: 这里应该构造要删除的文件的正确路径
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

    // todo: 以下四行代码没有什么作用
    // Check environment variables
    char home[10000];
    char path[10000];
    strcpy(path, getenv("PATH"));   // todo: 这里最好用 strncpy，避免潜在的缓冲区溢出
    strcpy(home, getenv("HOME"));   // todo: 这里最好用 strncpy，避免潜在的缓冲区溢出

    printf("PATH : %s\n", getenv("PATH"));  // todo: 这里应判断环境变量是否存在
    printf("HOME : %s\n", getenv("HOME"));  // todo: 这里应判断环境变量是否存在

    printf("Enter filter: ");
    char filter[32];        // todo: 这里应该设置足够多的缓冲区
    gets(filter);           // todo: gets 函数不安全，换一种用法
    
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
    char delete_conf[1];    // todo: 这里应该设置足够多的缓冲区
    gets(delete_conf);      // todo: gets 函数不安全，换一种用法
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
                    printf("OK     %s", current_filename); // todo: 为了输出美观，最好添加 "\n" 换行符
                else
                    printf("FAILED %s", current_filename); // todo: 为了输出美观，最好添加 "\n" 换行符
            }
        }
    } else {
        printf("Aborting.\n");
    }

    free(files);
    free(delete_choices);

    return 0;
}
