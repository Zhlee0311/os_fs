#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCKSIZE 1024
#define SIZE 1024000
#define END 65535
#define FREE 0 
#define ROOTBLOCKNUM 5
#define MAXOPENFILE 10
#define CTRL_Z 1
#define BLOCK_SIZE BLOCKSIZE
// FAT表项结构
typedef struct FAT {
    unsigned short id;
} fat;

// 文件控制块结构
typedef struct FCB {
    char filename[8];
    char exname[3];
    unsigned char attribute;
    unsigned short time;
    unsigned short date;
    unsigned short first;
    unsigned long length;
    char free;
} fcb;

// 引导块结构
typedef struct BLOCK0 {
    char information[200];
    unsigned short root;
    unsigned char* startblock;
} block0;

// 用户打开文件表结构
typedef struct USEROPEN {
    char filename[8];
    char exname[3];
    unsigned char attribute;
    unsigned short time;
    unsigned short date;
    unsigned short first;
    unsigned short length;
    char dir[80];
    int count;
    char fcbstate;
    char topenfile;
} useropen;

// 全局变量声明
unsigned char* myvhard;
useropen openfilelist[MAXOPENFILE];
int curdir;
char currentdir[80];
unsigned char* startp;
fat* fat1, * fat2;
block0* bootblock;

void my_format();
void my_exitsys();
void my_cd(char* dirname);
int do_read(int fd, int len, char* text);
void startsys() {
    myvhard = (unsigned char*)malloc(SIZE);
    if (!myvhard) {
        printf("Failed to allocate memory for virtual disk.\n");
        exit(1);
    }

    FILE* fp = fopen("filesys", "rb");
    if (fp) {
        fread(myvhard, SIZE, 1, fp);
        fclose(fp);
    } else {
        my_format();
    }


    bootblock = (block0*)myvhard;
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);


    fcb* root_fcb = (fcb*)(myvhard + bootblock->root * BLOCKSIZE);
    useropen* root = &openfilelist[0];
    strcpy(root->filename, ".");
    strcpy(root->exname, "");
    root->attribute = root_fcb->attribute;
    root->time = root_fcb->time;
    root->date = root_fcb->date;
    root->first = root_fcb->first;
    root->length = root_fcb->length;
    strcpy(root->dir, "/");
    root->count = 0;
    root->fcbstate = 0;
    root->topenfile = 1;


    curdir = 0;
    strcpy(currentdir, "/");
    for (int i = 1; i < MAXOPENFILE; i++) {
        openfilelist[i].topenfile = 0;
    }
}


void my_format() {
    if (!myvhard) {
        myvhard = (unsigned char*)malloc(SIZE);
    }


    bootblock = (block0*)myvhard;
    strcpy(bootblock->information, "Simple File System");
    bootblock->root = ROOTBLOCKNUM;
    bootblock->startblock = myvhard + 7 * BLOCKSIZE;


    int total_blocks = SIZE / BLOCKSIZE;//总共1000
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);


    for (int i = 0; i < total_blocks; i++) {
        if (i == 0 || (i >= 1 && i <= 4) || i == ROOTBLOCKNUM || i == ROOTBLOCKNUM + 1) {
            fat1[i].id = END;
            fat2[i].id = END;
        } else {
            fat1[i].id = FREE;
            fat2[i].id = FREE;
        }
    }

    fcb* root_dir = (fcb*)(myvhard + ROOTBLOCKNUM * BLOCKSIZE);
    memset(root_dir, 0, 2 * BLOCKSIZE);


    strcpy(root_dir[0].filename, ".");
    root_dir[0].attribute = 0x10; // 目录
    root_dir[0].first = ROOTBLOCKNUM;
    root_dir[0].length = 2 * BLOCKSIZE;
    root_dir[0].free = 1;

    strcpy(root_dir[1].filename, "..");
    root_dir[1].attribute = 0x10;
    root_dir[1].first = ROOTBLOCKNUM;
    root_dir[1].length = 2 * BLOCKSIZE;
    root_dir[1].free = 1;


    useropen* root = &openfilelist[0];
    strcpy(root->filename, ".");
    strcpy(root->exname, "");
    root->attribute = 0x10;
    root->time = 0;
    root->date = 0;
    root->first = ROOTBLOCKNUM;
    root->length = 2 * BLOCKSIZE;
    strcpy(root->dir, "/");
    root->count = 0;
    root->fcbstate = 0;
    root->topenfile = 1;
}


void my_exitsys() {
    if (myvhard) {
        FILE* fp = fopen("filesys", "wb");
        if (fp) {
            fwrite(myvhard, SIZE, 1, fp);
            fclose(fp);
        }
        free(myvhard);
        myvhard = NULL;
    }
}


int split_path(char* path, char** parts) {
    int count = 0;
    char* token = strtok(path, "/");
    while (token != NULL && count < MAXOPENFILE) {
        parts[count++] = token;
        token = strtok(NULL, "/");
    }
    return count;
}


int find_fcb(useropen* dir, char* name, fcb** found) {
    unsigned short block = dir->first;

    while (block != END) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);
        for (int j = 0; j < BLOCKSIZE / sizeof(fcb); j++) {
            if (dir_block[j].free == 1) {

                if (strcasecmp(dir_block[j].filename, name) == 0) {
                    *found = &dir_block[j];
                    return 0;
                }
            }
        }
        block = fat1[block].id;
    }
    return -1;
}

// void my_cd(char* dirname) {
//     if (dirname == NULL || strlen(dirname) == 0) {
//         printf("Current directory: %s\n", currentdir);
//         return;
//     }

//     if (strcmp(dirname, ".") == 0) {
//         return; // 当前目录，不做任何操作
//     }

//     if (strcmp(dirname, "..") == 0) {
//         // 如果是根目录，则不能再向上
//         if (strcmp(currentdir, "/") == 0) {
//             printf("Already at root directory\n");
//             return;
//         }

//         // 获取父目录路径
//         char parent_dir[80];
//         strcpy(parent_dir, currentdir);

//         // 移除末尾的'/'
//         if (parent_dir[strlen(parent_dir) - 1] == '/') {
//             parent_dir[strlen(parent_dir) - 1] = '\0';
//         }

//         // 找到上一级目录
//         char* last_slash = strrchr(parent_dir, '/');
//         if (last_slash != NULL) {
//             *last_slash = '\0';
//             if (strlen(parent_dir) == 0) {
//                 strcpy(parent_dir, "/"); // 回到根目录
//             } else {
//                 strcat(parent_dir, "/"); // 确保以/结尾
//             }
//         }

//         // 查找父目录FCB
//         fcb* parent_fcb = NULL;
//         if (find_fcb(&openfilelist[curdir], "..", &parent_fcb) != 0) {
//             printf("Error: Parent directory not found\n");
//             return;
//         }

//         // 更新当前目录
//         curdir = 0; // 默认回到根目录
//         for (int i = 0; i < MAXOPENFILE; i++) {
//             if (openfilelist[i].topenfile &&
//                 openfilelist[i].first == parent_fcb->first) {
//                 curdir = i;
//                 break;
//             }
//         }

//         strcpy(currentdir, parent_dir);
//         printf("Current directory changed to: %s\n", currentdir);
//         return;
//     }


//     // 处理路径中的斜杠
//     char normalized[80];
//     strcpy(normalized, dirname);
//     if (normalized[strlen(normalized) - 1] == '/') {
//         normalized[strlen(normalized) - 1] = '\0';
//     }

//     // 查找目录
//     fcb* target_fcb = NULL;


//     if (find_fcb(&openfilelist[curdir], normalized, &target_fcb) != 0) {
//         printf("Error: Directory '%s' not found in %s\n", normalized, currentdir);
//         return;
//     }

//     // 检查是否是目录
//     if (!(target_fcb->attribute & 0x10)) {
//         printf("Error: '%s' is not a directory\n", normalized);
//         return;
//     }

//     // 更新当前目录
//     char new_dir[80];
//     if (normalized[0] == '/') {
//         // 绝对路径
//         strcpy(new_dir, normalized);
//     } else {
//         // 相对路径
//         strcpy(new_dir, currentdir);
//         if (currentdir[strlen(currentdir) - 1] != '/') {
//             strcat(new_dir, "/");
//         }
//         strcat(new_dir, normalized);
//     }

//     // 确保路径以/结尾
//     if (new_dir[strlen(new_dir) - 1] != '/') {
//         strcat(new_dir, "/");
//     }

//     // 查找是否已经打开
//     int found = -1;
//     for (int j = 0; j < MAXOPENFILE; j++) {
//         if (openfilelist[j].topenfile &&
//             openfilelist[j].first == target_fcb->first) {
//             found = j;
//             break;
//         }
//     }

//     // 如果未打开则分配新表项
//     if (found == -1) {
//         for (int j = 0; j < MAXOPENFILE; j++) {
//             if (!openfilelist[j].topenfile) {
//                 strcpy(openfilelist[j].filename, target_fcb->filename);
//                 openfilelist[j].attribute = target_fcb->attribute;
//                 openfilelist[j].first = target_fcb->first;
//                 openfilelist[j].length = target_fcb->length;
//                 strcpy(openfilelist[j].dir, new_dir);
//                 openfilelist[j].topenfile = 1;
//                 found = j;
//                 break;
//             }
//         }
//         if (found == -1) {
//             printf("Error: Too many open files!\n");
//             return;
//         }
//     }

//     // 更新当前目录
//     curdir = found;
//     strcpy(currentdir, new_dir);
//     printf("Current directory changed to: %s\n", currentdir);
// }

void parse_path(char* path, char* res) {
    // 获取父目录的路径
    char parent_dir[80];
    if (strcmp(currentdir, "/") == 0) {
        strcpy(parent_dir, "/");
    } else {
        strcpy(parent_dir, currentdir);
        if (parent_dir[strlen(parent_dir) - 1] == '/') {
            parent_dir[strlen(parent_dir) - 1] = '\0'; // 移除末尾的'/'
        }
        char* last_slash = strrchr(parent_dir, '/');
        if (last_slash != NULL) {
            *last_slash = '\0'; // 移除最后一个'/'
            if (strlen(parent_dir) == 0) {
                strcpy(parent_dir, "/"); // 回到根目录
            } else {
                strcat(parent_dir, "/"); // 确保以/结尾
            }
        }
    }

    // 设置绝对路径栈的初始状态
    char stack[MAXOPENFILE][80];
    int top = -1;
    int start = 0; // 后续解析路径时对应的起始位置
    if (path[0] == '/') {
        strcpy(stack[++top], "/");
        printf("Push1: %s\n", stack[top]);
        start = 0;
    } else if (path[0] == '.' && path[1] == '.') {
        char* splited[MAXOPENFILE];
        int cnt = split_path(parent_dir, splited); // 分割父目录路径
        strcpy(stack[++top], "/");
        printf("Push2: %s\n", stack[top]);
        for (int i = 0; i < cnt; i++) {
            strcpy(stack[++top], splited[i]);
            printf("Push3: %s\n", stack[top]);
        }
        start = 1;
    } else {
        char* splited[MAXOPENFILE];
        int cnt = split_path(currentdir, splited); // 分割当前目录路径
        strcpy(stack[++top], "/");
        printf("Push4: %s\n", stack[top]);
        for (int i = 0; i < cnt; i++) {
            strcpy(stack[++top], splited[i]);
            printf("Push5: %s\n", stack[top]);
        }
        start = 0;
    }

    // 分割path
    char* parts[MAXOPENFILE];
    int count = split_path(path, parts); // 分割路径字符串

    // 逐层解析路径
    for (int i = start; i < count; i++) {
        if (strcmp(parts[i], ".") == 0) {
            continue;
        } else if (strcmp(parts[i], "..") == 0) {
            if (top > 0) {
                top--;
            }
        } else {
            strcpy(stack[++top], parts[i]);
            printf("Push6: %s\n", stack[top]);
        }
    }
    printf("Origin Res : %s\n", res);
    for (int i = 0; i <= top; i++) {
        printf("Stack[%d]: %s\n", i, stack[i]);
        if (i == 0 || i == 1) {
            strcat(res, stack[i]);
            printf("Res1: %s Cnt: %d\n", res, i);
        } else {
            strcat(res, "/");
            strcat(res, stack[i]);
            printf("Res2: %s Cnt: %d\n", res, i);
        }
    }
    strcat(res, "/"); // 确保以/结尾
}

void my_cd(char* dirname) {
    if (dirname == NULL || strlen(dirname) == 0) {
        printf("Current directory: %s\n", currentdir);
        return;
    }
    if (strcmp(dirname, ".") == 0) {
        return; // 当前目录，不做任何操作
    }

    if (strcmp(dirname, "..") == 0) {

        if (strcmp(currentdir, "/") == 0) {
            printf("Already at root directory\n");
            return;
        }
        // 获取父目录路径
        char parent_dir[80];
        strcpy(parent_dir, currentdir);

        // 移除末尾的'/'
        if (parent_dir[strlen(parent_dir) - 1] == '/') {
            parent_dir[strlen(parent_dir) - 1] = '\0';
        }

        // 找到上一级目录
        char* last_slash = strrchr(parent_dir, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
            if (strlen(parent_dir) == 0) {
                strcpy(parent_dir, "/"); // 回到根目录
            } else {
                strcat(parent_dir, "/"); // 确保以/结尾
            }
        }

        // 查找父目录FCB
        fcb* parent_fcb = NULL;
        if (find_fcb(&openfilelist[curdir], "..", &parent_fcb) != 0) {
            printf("Error: Parent directory not found\n");
            return;
        }

        // 更新当前目录
        curdir = 0; // 默认回到根目录
        for (int i = 0; i < MAXOPENFILE; i++) {
            if (openfilelist[i].topenfile &&
                openfilelist[i].first == parent_fcb->first) {
                curdir = i;
                break;
            }
        }

        strcpy(currentdir, parent_dir);
        printf("Current directory changed to: %s\n", currentdir);
        return;
    }

    // 解析路径为一条绝对路径
    char parsed_path[80];
    memset(parsed_path, 0, sizeof(parsed_path)); // 再次解析时务必清空原有内容
    parse_path(dirname, parsed_path);
    printf("Parsed path: %s\n", parsed_path);

    char* parts[MAXOPENFILE];
    char matched_path[80] = "/"; // 用于存储匹配的路径
    int count = split_path(parsed_path, parts); // 分割路径字符串

    useropen* buffer = (useropen*)malloc(sizeof(useropen)); // 设置缓冲区用于从根目录解析路径(相当于当前解析到的目录)
    buffer->first = openfilelist[0].first; // 根目录的起始块号

    for (int i = 0; i < count; i++) {
        fcb* found_fcb = NULL;
        if (find_fcb(buffer, parts[i], &found_fcb) < 0) {
            strcat(matched_path, parts[i]);
            strcat(matched_path, "/");
            printf("Directory not found: %s\n", matched_path);
            break;
        } else if (!(found_fcb->attribute & 0x10)) {
            strcat(matched_path, parts[i]);
            strcat(matched_path, "/");
            printf("Not a directory: %s\n", matched_path);
            break;
        } else {
            buffer->first = found_fcb->first; // 更新当前目录的起始块号, 其实find_fcb只需要这个，方便起见不复制其他参数
            if (i == count - 1) {
                // 如果最后一个部分匹配成功，更新当前目录
                int found = -1;
                for (int j = 0; j < MAXOPENFILE; j++) {
                    // 若此目录已经打开
                    if (openfilelist[j].topenfile && openfilelist[j].first == buffer->first) {
                        found = j;
                        break;
                    }
                }
                if (found == -1) {
                    // 如果没有打开，则在打开文件表中找到一个空闲的槽位
                    for (int j = 0; j < MAXOPENFILE; j++) {
                        if (!openfilelist[j].topenfile) {
                            strcpy(openfilelist[j].filename, found_fcb->filename);
                            openfilelist[j].attribute = found_fcb->attribute;
                            openfilelist[j].first = found_fcb->first;
                            openfilelist[j].length = found_fcb->length;
                            strcpy(openfilelist[j].dir, matched_path);
                            openfilelist[j].topenfile = 1;
                            found = j;
                            break;
                        }
                    }
                }
                if (found == -1) {
                    // 不能再打开更多的文件
                    printf("Error : Too many open files!\n");
                    break;
                }
                curdir = found; // 更新当前目录
                strcat(matched_path, parts[i]);
                strcat(matched_path, "/");
                strcpy(currentdir, matched_path); // 更新当前目录路径
                printf("Current directory changed to: %s\n", currentdir);
                free(buffer); // 释放缓冲区
                return;
            }
            strcat(matched_path, parts[i]);
            strcat(matched_path, "/");
        }
    }
    free(buffer); // 释放缓冲区
}


void my_ls() {
    useropen* current = &openfilelist[curdir];
    unsigned short block = current->first;
    int file_count = 0;
    int dir_count = 0;
    long total_size = 0;

    printf("\nDirectory of %s\n\n", currentdir);
    printf("%-12s %-4s %10s %12s %s\n", "Name", "Type", "Size", "Date", "Time");


    while (block != END) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);


        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++) {
            if (dir_block[i].free == 1) {

                if (strcmp(dir_block[i].filename, ".") == 0 ||
                    strcmp(dir_block[i].filename, "..") == 0) {
                    continue;
                }


                unsigned short date = dir_block[i].date;
                unsigned short time = dir_block[i].time;

                int year = (date >> 9) + 1980;
                int month = (date >> 5) & 0x0F;
                int day = date & 0x1F;

                int hour = time >> 11;
                int minute = (time >> 5) & 0x3F;
                int second = (time & 0x1F) * 2;


                if (dir_block[i].attribute & 0x10) { // 目录
                    printf("%-12s %-4s %10s %04d-%02d-%02d %02d:%02d:%02d\n",
                           dir_block[i].filename,
                           "<DIR>",
                           "",
                           year, month, day,
                           hour, minute, second);
                    dir_count++;
                } else {
                    printf("%-12s %-4s %10ld %04d-%02d-%02d %02d:%02d:%02d\n",
                           dir_block[i].filename,
                           strcmp(dir_block[i].exname, "") ? dir_block[i].exname : " ",
                           dir_block[i].length,
                           year, month, day,
                           hour, minute, second);
                    file_count++;
                    total_size += dir_block[i].length;
                }
            }
        }
        block = fat1[block].id;
    }


    printf("\n%12d File(s) %15ld bytes\n", file_count, total_size);
    printf("%12d Dir(s)\n", dir_count);
}


// 辅助函数：检查目录是否为空
bool is_dir_empty(useropen* dir) {
    unsigned short block = dir->first;
    int entry_count = 0;

    while (block != END) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++) {
            if (dir_block[i].free == 1) {
                // 跳过 "." 和 ".." 目录项
                if (strcmp(dir_block[i].filename, ".") != 0 &&
                    strcmp(dir_block[i].filename, "..") != 0) {
                    return false;
                }
                entry_count++;
            }
        }
        block = fat1[block].id;
    }
    return (entry_count == 2); // 只有 . 和 .. 两个条目
}


void my_mkdir(char* dirname) {

    if (strlen(dirname) > 8) {
        printf("Directory name too long (max 8 characters)\n");
        return;
    }

    useropen* current = &openfilelist[curdir];


    fcb* existing = NULL;
    if (find_fcb(current, dirname, &existing) == 0) {
        printf("Directory already exists: %s\n", dirname);
        return;
    }


    fcb* free_fcb = NULL;
    unsigned short block = current->first;
    int found = 0;

    while (block != END && !found) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++) {
            if (dir_block[i].free == 0) {
                free_fcb = &dir_block[i];
                found = 1;
                break;
            }
        }
        if (!found) block = fat1[block].id;
    }

    if (!free_fcb) {

        unsigned short new_block = 0;
        for (int i = 5; i < SIZE / BLOCKSIZE; i++) {
            if (fat1[i].id == FREE) {
                new_block = i;
                break;
            }
        }
        if (new_block == 0) {
            printf("No free space available\n");
            return;
        }


        fat1[new_block].id = END;
        fat2[new_block].id = END;


        block = current->first;
        while (fat1[block].id != END) {
            block = fat1[block].id;
        }
        fat1[block].id = new_block;
        fat2[block].id = new_block;


        fcb* new_dir_block = (fcb*)(myvhard + new_block * BLOCKSIZE);
        memset(new_dir_block, 0, BLOCKSIZE);
        free_fcb = &new_dir_block[0];
    }

    // 分配新目录的空间
    unsigned short new_dir_block = 0;
    for (int i = 5; i < SIZE / BLOCKSIZE; i++) {
        if (fat1[i].id == FREE) {
            new_dir_block = i;
            break;
        }
    }
    if (new_dir_block == 0) {
        printf("No free space available\n");
        return;
    }


    fat1[new_dir_block].id = END;
    fat2[new_dir_block].id = END;
    fcb* new_dir = (fcb*)(myvhard + new_dir_block * BLOCKSIZE);
    memset(new_dir, 0, BLOCKSIZE);


    strcpy(new_dir[0].filename, ".");
    new_dir[0].attribute = 0x10;
    new_dir[0].first = new_dir_block;
    new_dir[0].length = BLOCKSIZE;
    new_dir[0].free = 1;

    strcpy(new_dir[1].filename, "..");
    new_dir[1].attribute = 0x10;
    new_dir[1].first = current->first;
    new_dir[1].length = current->length;
    new_dir[1].free = 1;


    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    unsigned short date = ((tm_now->tm_year - 80) << 9) |
        ((tm_now->tm_mon + 1) << 5) |
        tm_now->tm_mday;
    unsigned short time = (tm_now->tm_hour << 11) |
        (tm_now->tm_min << 5) |
        (tm_now->tm_sec / 2);


    strcpy(free_fcb->filename, dirname);
    free_fcb->attribute = 0x10;
    free_fcb->first = new_dir_block;
    free_fcb->length = BLOCKSIZE;
    free_fcb->time = time;
    free_fcb->date = date;
    free_fcb->free = 1;


    current->fcbstate = 1;

    printf("Directory created: %s\n", dirname);
}


void my_rmdir(char* dirname) {
    useropen* current = &openfilelist[curdir];


    fcb* target_fcb = NULL;
    if (find_fcb(current, dirname, &target_fcb) != 0) {
        printf("Directory not found: %s\n", dirname);
        return;
    }


    if (!(target_fcb->attribute & 0x10)) {
        printf("Not a directory: %s\n", dirname);
        return;
    }


    useropen temp_dir;
    strcpy(temp_dir.filename, dirname);
    temp_dir.first = target_fcb->first;
    temp_dir.length = target_fcb->length;

    if (!is_dir_empty(&temp_dir)) {
        printf("Directory not empty: %s\n", dirname);
        return;
    }


    unsigned short block = target_fcb->first;
    while (block != END) {
        unsigned short next = fat1[block].id;
        fat1[block].id = FREE;
        fat2[block].id = FREE;
        block = next;
    }

    target_fcb->free = 0;


    current->fcbstate = 1;

    printf("Directory removed: %s\n", dirname);
}

int my_create(char* filename) {

    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen* current = &openfilelist[curdir];


    fcb* existing = NULL;
    if (find_fcb(current, filename, &existing) == 0) {
        printf("File already exists: %s\n", filename);
        return -1;
    }


    fcb* free_fcb = NULL;
    unsigned short block = current->first;
    int found = 0;

    while (block != END && !found) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++) {
            if (dir_block[i].free == 0) {
                free_fcb = &dir_block[i];
                found = 1;
                break;
            }
        }
        if (!found) block = fat1[block].id;
    }

    if (!free_fcb) {

        unsigned short new_block = 0;
        for (int i = ROOTBLOCKNUM + 1; i < SIZE / BLOCKSIZE; i++) {
            if (fat1[i].id == FREE) {
                new_block = i;
                break;
            }
        }
        if (new_block == 0) {
            printf("No free space available\n");
            return -1;
        }


        fat1[new_block].id = END;
        fat2[new_block].id = END;


        block = current->first;
        while (fat1[block].id != END) {
            block = fat1[block].id;
        }
        fat1[block].id = new_block;
        fat2[block].id = new_block;


        fcb* new_dir_block = (fcb*)(myvhard + new_block * BLOCKSIZE);
        memset(new_dir_block, 0, BLOCKSIZE);
        free_fcb = &new_dir_block[0];
    }


    unsigned short new_file_block = 0;
    for (int i = ROOTBLOCKNUM + 1; i < SIZE / BLOCKSIZE; i++) {
        if (fat1[i].id == FREE) {
            new_file_block = i;
            break;
        }
    }
    if (new_file_block == 0) {
        printf("No free space available\n");
        return -1;
    }


    fat1[new_file_block].id = END;
    fat2[new_file_block].id = END;
    memset(myvhard + new_file_block * BLOCKSIZE, 0, BLOCKSIZE);


    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    unsigned short date = ((tm_now->tm_year - 80) << 9) |
        ((tm_now->tm_mon + 1) << 5) |
        tm_now->tm_mday;
    unsigned short time = (tm_now->tm_hour << 11) |
        (tm_now->tm_min << 5) |
        (tm_now->tm_sec / 2);


    strcpy(free_fcb->filename, filename);
    free_fcb->attribute = 0x00; // 文件属性
    free_fcb->first = new_file_block;
    free_fcb->length = 0;
    free_fcb->time = time;
    free_fcb->date = date;
    free_fcb->free = 1;


    int fd = -1;
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (!openfilelist[i].topenfile) {
            strcpy(openfilelist[i].filename, filename);
            openfilelist[i].attribute = 0x00;
            openfilelist[i].first = new_file_block;
            openfilelist[i].length = 0;
            strcpy(openfilelist[i].dir, currentdir);
            openfilelist[i].count = 0;
            openfilelist[i].fcbstate = 0;
            openfilelist[i].topenfile = 1;
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        printf("Too many open files!\n");
        return -1;
    }


    current->fcbstate = 1;

    printf("File created: %s (fd=%d)\n", filename, fd);
    return fd;
}


void my_rm(char* filename) {
    useropen* current = &openfilelist[curdir];


    fcb* target_fcb = NULL;
    if (find_fcb(current, filename, &target_fcb) != 0) {
        printf("File not found: %s\n", filename);
        return;
    }


    if (target_fcb->attribute & 0x10) {
        printf("Not a regular file: %s (use my_rmdir for directories)\n", filename);
        return;
    }


    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile &&
            openfilelist[i].first == target_fcb->first) {
            printf("File is currently open (fd=%d), close it first\n", i);
            return;
        }
    }


    unsigned short block = target_fcb->first;
    while (block != END) {
        unsigned short next = fat1[block].id;
        fat1[block].id = FREE;
        fat2[block].id = FREE;
        block = next;
    }


    target_fcb->free = 0;


    current->fcbstate = 1;

    printf("File removed: %s\n", filename);
}

int my_open(char* filename) {

    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen* current = &openfilelist[curdir];


    fcb* file_fcb = NULL;
    if (find_fcb(current, filename, &file_fcb) != 0) {
        printf("File not found: %s\n", filename);
        return -1;
    }


    if (file_fcb->attribute & 0x10) {
        printf("%s is a directory (use my_cd)\n", filename);
        return -1;
    }


    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile &&
            openfilelist[i].first == file_fcb->first &&
            strcmp(openfilelist[i].dir, currentdir) == 0) {
            printf("File already opened (fd=%d)\n", i);
            return i;
        }
    }


    int fd = -1;
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (!openfilelist[i].topenfile) {

            strcpy(openfilelist[i].filename, file_fcb->filename);
            openfilelist[i].attribute = file_fcb->attribute;
            openfilelist[i].first = file_fcb->first;
            openfilelist[i].length = file_fcb->length;
            strcpy(openfilelist[i].dir, currentdir);
            openfilelist[i].count = 0;  // 初始文件指针位置
            openfilelist[i].fcbstate = 0;
            openfilelist[i].topenfile = 1;
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        printf("Too many open files!\n");
        return -1;
    }

    printf("File opened: %s (fd=%d)\n", filename, fd);
    return fd;
}

void my_close(char* filename) {

    if (strlen(filename) > 8) {
        printf("Invalid filename: %s\n", filename);
        return;
    }

    int close_count = 0;
    int current_dir_len = strlen(currentdir);

    for (int fd = 0; fd < MAXOPENFILE; fd++) {
        if (openfilelist[fd].topenfile &&
            strcmp(openfilelist[fd].filename, filename) == 0 &&
            strncmp(openfilelist[fd].dir, currentdir, current_dir_len) == 0) {


            if (openfilelist[fd].fcbstate) {

                useropen* parent = NULL;
                for (int i = 0; i < MAXOPENFILE; i++) {
                    if (openfilelist[i].topenfile &&
                        strcmp(openfilelist[i].dir, openfilelist[fd].dir) == 0) {
                        parent = &openfilelist[i];
                        break;
                    }
                }

                if (parent) {

                    fcb* file_fcb = NULL;
                    if (find_fcb(parent, filename, &file_fcb) == 0) {

                        file_fcb->length = openfilelist[fd].length;
                        file_fcb->time = openfilelist[fd].time;
                        file_fcb->date = openfilelist[fd].date;


                        parent->fcbstate = 1;
                    }
                }
            }


            memset(&openfilelist[fd], 0, sizeof(useropen));
            openfilelist[fd].topenfile = 0;
            close_count++;
        }
    }


    if (close_count > 0) {
        printf("Closed %d instance(s) of %s\n", close_count, filename);
    } else {
        printf("File not open: %s\n", filename);
    }
}





int do_write(int fd, char* text, int len, char wstyle);

// int my_write(int fd) {
//     // 检查文件描述符有效性
//     if (fd < 0 || fd >= MAXOPENFILE || !openfilelist[fd].topenfile) {
//         printf("Invalid file descriptor: %d\n", fd);
//         return -1;
//     }

//     // 检查文件属性
//     if (openfilelist[fd].attribute & 0x10) {
//         printf("Cannot write to a directory\n");
//         return -1;
//     }

//     // 选择写入方式
//     printf("Select write style:\n");
//     printf("1. Truncate write (overwrite entire file)\n");
//     printf("2. Overwrite write (write from current position)\n");
//     printf("3. Append write (write at end of file)\n");
//     printf("Your choice (1-3): ");

//     char wstyle;
//     scanf("%hhd", &wstyle);
//     getchar(); // 消耗换行符

//     if (wstyle < 1 || wstyle > 3) {
//         printf("Invalid write style selection\n");
//         return -1;
//     }

//     // 设置文件指针位置
//     switch (wstyle) {
//         case 1: // 截断写
//             openfilelist[fd].count = 0;
//             openfilelist[fd].length = 0;
//             break;
//         case 2: // 覆盖写
//             // 保持当前count位置不变
//             break;
//         case 3: // 追加写
//             openfilelist[fd].count = openfilelist[fd].length;

//             break;
//     }

//     // 读取用户输入
//     printf("Enter content (end with Ctrl+Z):\n");

//     char buffer[BLOCK_SIZE];
//     char input_buffer[BLOCK_SIZE * 4]; // 临时存储区，可容纳4个块的内容
//     int input_len = 0;
//     char ch;

//     while ((ch = getchar()) != CTRL_Z && input_len < sizeof(input_buffer) - 1) {
//         input_buffer[input_len++] = ch;
//     }
//     input_buffer[input_len] = '\0';

//     // 调用实际写入函数
//     int write_len = do_write(fd, input_buffer, input_len, wstyle);
//     if (write_len < 0) {
//         printf("Write failed\n");
//         return -1;
//     }

//     // 更新文件长度
//     if (wstyle == 1 ) {
//         openfilelist[fd].length += write_len;
//     }

//     printf("File length: %d, Read pointer: %d\n", openfilelist[fd].length, openfilelist[fd].count);
//     // 标记文件为已修改
//     openfilelist[fd].fcbstate = 1;

//     printf("Successfully wrote %d bytes to file (fd=%d)\n", write_len, fd);
//     return write_len;
// }

// int do_write(int fd, char* text, int len, char wstyle) {
//     useropen* file = &openfilelist[fd];
//     int bytes_written = 0;
//     int remaining = len;
//     char* ptr = text;

//     // 计算起始位置
//     unsigned int logical_block = file->count / BLOCK_SIZE;
//     unsigned int block_offset = file->count % BLOCK_SIZE;

//     // 遍历找到起始块
//     unsigned short current_block = file->first;
//     for (unsigned int i = 0; i < logical_block && current_block != END; i++) {
//         current_block = fat1[current_block].id;
//     }

//     if (current_block == END && logical_block > 0) {
//         printf("Error: Invalid file position\n");
//         return -1;
//     }

//     // 如果是截断写且需要新块，先释放原有块
//     if (wstyle == 1) {
//         unsigned short block = file->first;
//         while (block != END) {
//             unsigned short next = fat1[block].id;
//             fat1[block].id = FREE;
//             fat2[block].id = FREE;
//             block = next;
//         }
//         file->first = END;
//         current_block = END;
//         file->count = 0;
//         block_offset = 0;
//     }

//     // 写入循环
//     while (remaining > 0) {
//         // 需要新块的情况
//         if (current_block == END || block_offset == 0) {
//             // 分配新块
//             unsigned short new_block = 0;
//             for (int i = ROOTBLOCKNUM + 1; i < SIZE / BLOCKSIZE; i++) {
//                 if (fat1[i].id == FREE) {
//                     new_block = i;
//                     break;
//                 }
//             }

//             if (new_block == 0) {
//                 printf("Error: No free space available\n");
//                 break;
//             }

//             // 链接新块
//             fat1[new_block].id = END;
//             fat2[new_block].id = END;

//             if (current_block == END) {
//                 file->first = new_block;
//             } else {
//                 fat1[current_block].id = new_block;
//                 fat2[current_block].id = new_block;
//             }

//             current_block = new_block;
//             block_offset = 0;

//             // 初始化新块
//             memset(myvhard + current_block * BLOCK_SIZE, 0, BLOCK_SIZE);
//         }

//         // 计算本次写入长度
//         int write_size = BLOCK_SIZE - block_offset;
//         if (write_size > remaining) {
//             write_size = remaining;
//         }

//         // 执行写入
//         char* block_ptr = (char*)(myvhard + current_block * BLOCK_SIZE + block_offset);
//         memcpy(block_ptr, ptr, write_size);

//         // 更新指针和计数器
//         ptr += write_size;
//         remaining -= write_size;
//         bytes_written += write_size;
//         file->count += write_size;
//         block_offset += write_size;

//         // 如果块写满，移动到下一个块
//         if (block_offset >= BLOCK_SIZE) {
//             current_block = fat1[current_block].id;
//             block_offset = 0;
//         }
//     }

//     // 更新文件长度（如果需要）
//     if (file->count > file->length) {
//         file->length = file->count;
//     }
//     time_t now = time(NULL);
//     struct tm *tm_now = localtime(&now);
//     openfilelist[fd].date = ((tm_now->tm_year - 80) << 9) | 
//                            ((tm_now->tm_mon + 1) << 5) | 
//                            tm_now->tm_mday;
//     openfilelist[fd].time = (tm_now->tm_hour << 11) | 
//                            (tm_now->tm_min << 5) | 
//                            (tm_now->tm_sec / 2);
//     return bytes_written;
// }


#define CTRL_Z 1 
#define MAX_FILE_SIZE (10 * 1024 * 1024) 
int safe_append_write(int fd, char* text, int len);
int my_write(int fd) {

    if (fd < 0 || fd >= MAXOPENFILE || !openfilelist[fd].topenfile) {
        printf("Invalid file descriptor: %d\n", fd);
        return -1;
    }

    if (openfilelist[fd].attribute & 0x10) {
        printf("Cannot write to a directory\n");
        return -1;
    }


    printf("Select write style:\n");
    printf("1. Overwrite (clear and write)\n");
    printf("2. Modify (write at position)\n");
    printf("3. Append (safe read-append-write)\n");
    printf("Your choice (1-3): ");

    char wstyle;
    if (scanf("%hhd", &wstyle) != 1 || wstyle < 1 || wstyle > 3) {
        printf("Invalid write style selection\n");
        return -1;
    }
    getchar();


    printf("Enter content (end with Ctrl+Z):\n");

    char* input_buffer = malloc(MAX_FILE_SIZE + 1);
    if (!input_buffer) {
        printf("Memory allocation failed\n");
        return -1;
    }

    int input_len = 0;
    char ch;
    while ((ch = getchar()) != CTRL_Z && input_len < MAX_FILE_SIZE) {
        input_buffer[input_len++] = ch;
    }
    input_buffer[input_len] = '\0';


    int write_len;
    if (wstyle == 3) {

        write_len = safe_append_write(fd, input_buffer, input_len);
    } else {

        write_len = do_write(fd, input_buffer, input_len, wstyle);
    }

    free(input_buffer);

    if (write_len < 0) {
        printf("Write failed\n");
        return -1;
    }


    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    openfilelist[fd].date = ((tm_now->tm_year - 80) << 9) |
        ((tm_now->tm_mon + 1) << 5) |
        tm_now->tm_mday;
    openfilelist[fd].time = (tm_now->tm_hour << 11) |
        (tm_now->tm_min << 5) |
        (tm_now->tm_sec / 2);


    openfilelist[fd].fcbstate = 1;

    printf("Successfully wrote %d bytes (fd=%d)\n", write_len, fd);
    return write_len;
}


int do_write(int fd, char* text, int len, char wstyle) {
    useropen* file = &openfilelist[fd];


    if ((wstyle == 1 ? len : file->length + len) > MAX_FILE_SIZE) {
        printf("Error: File size exceeds limit\n");
        return -1;
    }


    switch (wstyle) {
        case 1:
            file->count = 0;
            file->length = 0;
            break;
        case 2: // 修改写
            if (file->count > file->length) {
                file->count = file->length;
            }
            break;
        case 3:
            printf("Invalid mode for do_write\n");
            return -1;
    }


    int bytes_written = 0;
    unsigned short current_block = file->first;
    unsigned int block_offset = file->count % BLOCK_SIZE;


    if (wstyle == 1 && file->first != END) {
        unsigned short block = file->first;
        while (block != END) {
            unsigned short next = fat1[block].id;
            fat1[block].id = FREE;
            fat2[block].id = FREE;
            block = next;
        }
        file->first = END;
        current_block = END;
    }


    if (wstyle != 1) {
        current_block = file->first;
        unsigned int target_block = file->count / BLOCK_SIZE;
        for (unsigned int i = 0; i < target_block && current_block != END; i++) {
            current_block = fat1[current_block].id;
        }
        if (current_block == END && target_block > 0) {
            printf("Error: Invalid file position\n");
            return -1;
        }
    }


    char* ptr = text;
    int remaining = len;
    while (remaining > 0) {

        if (current_block == END || block_offset == 0) {
            unsigned short new_block = 0;
            for (int i = ROOTBLOCKNUM + 1; i < SIZE / BLOCKSIZE; i++) {
                if (fat1[i].id == FREE) {
                    new_block = i;
                    break;
                }
            }
            if (new_block == 0) {
                printf("Error: No free space\n");
                break;
            }

            fat1[new_block].id = END;
            fat2[new_block].id = END;

            if (current_block == END) {
                file->first = new_block;
            } else {
                fat1[current_block].id = new_block;
                fat2[current_block].id = new_block;
            }

            current_block = new_block;
            block_offset = 0;
            memset(myvhard + current_block * BLOCKSIZE, 0, BLOCKSIZE);
        }


        int write_size = BLOCKSIZE - block_offset;
        if (write_size > remaining) write_size = remaining;


        char* block_ptr = (char*)(myvhard + current_block * BLOCKSIZE + block_offset);
        memcpy(block_ptr, ptr, write_size);

        ptr += write_size;//指针移动
        remaining -= write_size;
        bytes_written += write_size;
        file->count += write_size;
        block_offset += write_size;

        if (file->count > file->length) {
            file->length = file->count;
        }

        if (block_offset >= BLOCKSIZE) {
            current_block = fat1[current_block].id;
            block_offset = 0;
        }
    }

    return bytes_written;
}

int safe_append_write(int fd, char* text, int len) {
    useropen* file = &openfilelist[fd];


    char* old_content = NULL;
    int old_length = file->length;

    if (old_length > 0) {
        old_content = (char*)malloc(old_length);
        if (!old_content) return -1;

        int old_pos = file->count;
        file->count = 0;
        int read_len = do_read(fd, old_length, old_content);
        file->count = old_pos;

        if (read_len != old_length) {
            free(old_content);
            return -1;
        }
    }


    char* new_content = (char*)malloc(old_length + len);
    if (!new_content) {
        if (old_content) free(old_content);
        return -1;
    }

    if (old_content) {
        memcpy(new_content, old_content, old_length);
        free(old_content);
    }
    memcpy(new_content + old_length, text, len);


    int result = do_write(fd, new_content, old_length + len, 1);
    free(new_content);


    file->count = file->length;
    return result;
}

int my_read(int fd, int len) {
    // 检查文件描述符有效性
    if (fd < 0 || fd >= MAXOPENFILE || !openfilelist[fd].topenfile) {
        printf("Invalid file descriptor: %d\n", fd);
        return -1;
    }

    // 检查是否为目录，目录不可读
    if (openfilelist[fd].attribute & 0x10) {
        printf("Cannot read a directory\n");
        return -1;
    }

    // 计算实际可读长度（防止越界）
    int available = openfilelist[fd].length - openfilelist[fd].count;
    if (available <= 0) {
        printf("End of file reached\n");
        return 0;
    }
    if (len > available) {
        len = available;
    }

    // 分配缓冲区
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) {
        printf("Memory allocation failed\n");
        return -1;
    }

    // 调用 do_read 执行读取
    int read_len = do_read(fd, len, buffer);
    if (read_len > 0) {
        buffer[read_len] = '\0'; // 确保字符串安全
        printf("Read content: %s\n", buffer);
    }

    free(buffer);
    return read_len;
}

int do_read(int fd, int len, char* text) {
    useropen* file = &openfilelist[fd];
    int bytes_read = 0;
    int remaining = len;
    char* ptr = text;

    // 计算起始逻辑块号和偏移量
    unsigned int logical_block = file->count / BLOCK_SIZE;
    unsigned int block_offset = file->count % BLOCK_SIZE;

    // 遍历找到起始块
    unsigned short current_block = file->first;
    for (unsigned int i = 0; i < logical_block && current_block != END; i++) {
        current_block = fat1[current_block].id;
    }

    if (current_block == END) {
        printf("Error: Invalid file position\n");
        return -1;
    }

    // 读取数据
    while (remaining > 0) {
        // 读取当前块数据
        char* block_ptr = (char*)(myvhard + current_block * BLOCK_SIZE);

        // 计算本次读取长度
        int read_size = BLOCK_SIZE - block_offset;
        if (read_size > remaining) {
            read_size = remaining;
        }

        // 复制数据到用户缓冲区
        memcpy(ptr, block_ptr + block_offset, read_size);

        // 更新指针和计数器
        ptr += read_size;
        remaining -= read_size;
        bytes_read += read_size;
        file->count += read_size;
        block_offset += read_size;

        // 如果块读完，移动到下一个块
        if (block_offset >= BLOCK_SIZE) {
            current_block = fat1[current_block].id;
            block_offset = 0;
            if (current_block == END) {
                break;
            }
        }
    }

    return bytes_read;
}

int my_rewind(int fd) {
    if (fd < 0 || fd >= MAXOPENFILE || !openfilelist[fd].topenfile)
        return -1;
    openfilelist[fd].count = 0;
    return 0;
}

int main() {
    startsys();
    char cmd[20];
    char path[80];
    char arg[80];
    int fd;
    while (1) {
        printf("\n%s> ", currentdir);
        scanf("%s", cmd);
        if (strcmp(cmd, "format") == 0) {
            my_format();
            printf("File system formatted.\n");
        } else if (strcmp(cmd, "open") == 0) {
            scanf("%s", arg);
            fd = my_open(arg);
        } else if (strcmp(cmd, "close") == 0) {
            scanf("%s", arg);
            my_close(arg);
        } else if (strcmp(cmd, "exitsys") == 0) {
            my_exitsys();
            printf("Exiting system.\n");
            break;
        } else if (strcmp(cmd, "create") == 0) {
            scanf("%s", arg);
            my_create(arg);
        } else if (strcmp(cmd, "rm") == 0) {
            scanf("%s", arg);
            my_rm(arg);
        } else if (strcmp(cmd, "mkdir") == 0) {
            scanf("%s", path);
            my_mkdir(path);
        } else if (strcmp(cmd, "rmdir") == 0) {
            scanf("%s", path);
            my_rmdir(path);
        } else if (strcmp(cmd, "ls") == 0) {
            my_ls();
        } else if (strcmp(cmd, "cd") == 0) {
            scanf("%s", path);
            // printf("%s",path);
            my_cd(path);
        } else if (strcmp(cmd, "write") == 0) {
            scanf("%d", &fd);
            my_write(fd);
        } else if (strcmp(cmd, "read") == 0) {
            int len;
            scanf("%d %d", &fd, &len);
            my_read(fd, len);
        } else if (strcmp(cmd, "rewind") == 0) {
            scanf("%d", &fd);
            my_rewind(fd);
        } else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}