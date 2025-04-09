#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCKSIZE 1024 // 每个块的大小为1024B
#define SIZE 1024000  // 虚拟磁盘大小为1024000B
#define END 65535   // FAT结束标志
#define FREE 0      // FAT空闲标志
#define ROOTBLOCKNUM 5  
#define MAXOPENFILE 10
#define CTRL_Z 26  
#define BLOCK_SIZE BLOCKSIZE 

// FAT表项结构
typedef struct FAT {
    unsigned short id;
} fat;
// ... => x => fat[x].id => ... => END
// 若fat[x].id = END，则表示x是文件的最后一个块
// 若fat[x].id = FREE，则表示x块未使用


// 文件控制块结构
typedef struct FCB {
    char filename[8];       // 文件名
    char exname[3];         // 拓展名
    unsigned char attribute;// 文件属性，区分目录文件和普通文件
    unsigned short time;    // 创建时间
    unsigned short date;    // 创建日期
    unsigned short first;   // 起始块号
    unsigned long length;   // 文件长度（B）
    char free;              // 是否被使用，1--使用，0--未使用
} fcb;

// 引导块结构
typedef struct BLOCK0 {
    char information[200];  // "Simple File System"
    unsigned short root;    // 根目录起始块号
    unsigned char* startblock; // 数据区起始地址
} block0;

// 用户打开文件表结构
typedef struct USEROPEN {
    char filename[8];        // 文件名
    char exname[3];          // 拓展名
    unsigned char attribute; // 文件属性，区分目录文件和普通文件
    unsigned short time;     // 创建时间
    unsigned short date;     // 创建日期
    unsigned short first;    // 起始块号
    unsigned short length;   // 文件长度（B）
    char dir[80];            // 此文件的路径
    int count;               // 对这个文件的读写指针
    char fcbstate;           // FCB是否被修改
    char topenfile;          // 0-未使用，1-使用
} useropen;

// 全局变量声明
unsigned char* myvhard; // 虚拟磁盘指针，即分配的内存首地址
useropen openfilelist[MAXOPENFILE]; // 打开文件的表，最多支持10个文件
int curdir;        // 当前所在的目录文件的下标
char currentdir[80]; // 当前目录路径，/HDU/OS_lab/dir1...
unsigned char* startp;
fat* fat1, * fat2;  // FAT1和FAT2指针, 第x个磁盘块的下一块是fat[x].id
block0* bootblock; // 引导块指针，指向虚拟磁盘的第一个块，

void my_format();
void my_exitsys();
void my_cd(char* dirname);

void startsys() {
    // 分配一块1024000B的内存用作虚拟磁盘
    myvhard = (unsigned char*)malloc(SIZE);
    if (!myvhard) {
        printf("Failed to allocate memory for virtual disk.\n");
        exit(1);
    }
    // 读取虚拟磁盘文件
    FILE* fp = fopen("filesys", "rb");
    if (fp) {
        // 读取 1 个 1024000B 的块到内存中，myvhard作为首地址
        fread(myvhard, SIZE, 1, fp);
        fclose(fp);
    } else {
        // 若存储虚拟磁盘的文件不存在，则进行格式化
        my_format();
    }

    // 以block0的方式解释首地址，即首地址为引导块
    bootblock = (block0*)myvhard;
    // fat1的地址 = 内存首地址 + 1 * BLOCKSIZE  
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    // fat2的地址 = 内存首地址 + 3 * BLOCKSIZE     
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);

    /*
    0  引导块
    1-2 fat1
    3-4 fat2
    5-6 根目录块，用于存放目录文件和普通文件的fcb
    7-... 数据区
    */

    // 初始化用户打开文件表项0（根目录）
    // 获取第5个磁盘块的首地址，并以fcb的方式解释（由于目录文件存储本级和上级的fcb，因此root_fcb[0]为‘.’的fcb，root_fcb[1]为‘..’的fcb）
    fcb* root_fcb = (fcb*)(myvhard + bootblock->root * BLOCKSIZE);
    // 根据根目录fcb的信息，初始化用户打开文件表项0，即打开根目录并完善相关信息
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

    // 初始化当前目录信息
    curdir = 0;
    strcpy(currentdir, "/");
    // 将其他打开文件表项初始化为未使用
    for (int i = 1; i < MAXOPENFILE; i++) {
        openfilelist[i].topenfile = 0;
    }
}

// 格式化虚拟磁盘
void my_format() {
    // 分配一块1024000B的内存用作虚拟磁盘
    if (!myvhard) {
        myvhard = (unsigned char*)malloc(SIZE);
    }

    // 初始化引导块
    bootblock = (block0*)myvhard;
    strcpy(bootblock->information, "Simple File System");
    bootblock->root = ROOTBLOCKNUM;                    // 根目录起始块号，即第5个块
    bootblock->startblock = myvhard + 7 * BLOCKSIZE;   // 数据区起始地址

    // 初始化FAT表
    int total_blocks = SIZE / BLOCKSIZE;
    fat1 = (fat*)(myvhard + BLOCKSIZE);
    fat2 = (fat*)(myvhard + 3 * BLOCKSIZE);

    // 标记系统使用的块
    for (int i = 0; i < total_blocks; i++) {
        if (i >= 0 && i <= ROOTBLOCKNUM + 1) {
            fat1[i].id = END;
            fat2[i].id = END;
        } else {
            fat1[i].id = FREE;
            fat2[i].id = FREE;
        }
    }

    // 磁盘第5块的起始地址，将其作为根目录的fcb，并初始化
    fcb* root_dir = (fcb*)(myvhard + ROOTBLOCKNUM * BLOCKSIZE);
    memset(root_dir, 0, 2 * BLOCKSIZE);

    // 目录文件有两个基本fcb
    // 假设目录文件的起始地址为 root_dir, 则root_dir[0]为 ‘.’ 的fcb， root_dir[1]为 ‘..’ 的fcb
    strcpy(root_dir[0].filename, ".");
    root_dir[0].attribute = 0x10; // 目录文件
    root_dir[0].first = ROOTBLOCKNUM;     // 5-6块为根目录
    root_dir[0].length = 2 * BLOCKSIZE;
    root_dir[0].free = 1;

    strcpy(root_dir[1].filename, "..");
    root_dir[1].attribute = 0x10;
    root_dir[1].first = ROOTBLOCKNUM;
    root_dir[1].length = 2 * BLOCKSIZE;
    root_dir[1].free = 1;

    // 设置用户打开文件表项0
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

// 退出并保存文件系统
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

/**
 * @brief 分割路径字符串为各个部分
 * @param path 输入的路径字符串
 * @param parts 分割后的字符串数组
 * @return 返回分割的部分数量
 */
 //  example:
 //  path = "/dir1/dir2/file.txt"  
 //  parts = ["dir1", "dir2", "file.txt"]
 //  count = 3
int split_path(char* path, char** parts) {
    int count = 0;
    char* token = strtok(path, "/");
    while (token != NULL && count < MAXOPENFILE) {
        parts[count++] = token;
        token = strtok(NULL, "/");
    }
    return count;
}

/**
 * @brief 在给定目录中查找 普通文件 或 目录文件 的fcb
 * @param dir 一个目录文件的指针
 * @param name 要查找的文件名
 * @param found 找到的fcb* 的指针
 */
int find_fcb(useropen* dir, char* name, fcb** found) {
    // 当前目录文件所在的起始块号
    unsigned short block = dir->first;
    while (block != END) {
        // 以fcb的方式解释 <= 块首地址  <= 起始块号
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);
        // 遍历块中的所有fcb
        for (int j = 0; j < BLOCKSIZE / sizeof(fcb); j++) {
            if (dir_block[j].free == 1) {
                // 比较文件名(不区分大小写)
                if (strcasecmp(dir_block[j].filename, name) == 0) {
                    *found = &dir_block[j];
                    return 0; // 找到
                }
            }
        }
        block = fat1[block].id;
    }
    return -1; // 未找到
}

/**
 * @brief 解析路径字符串
 * @param path 输入的路径字符串
 * @param res 解析后的结果字符串
 */
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
        //printf("Push1: %s\n", stack[top]);
        start = 0;
    } else if (path[0] == '.' && path[1] == '.') {
        char* splited[MAXOPENFILE];
        int cnt = split_path(parent_dir, splited); // 分割父目录路径
        strcpy(stack[++top], "/");
        //printf("Push2: %s\n", stack[top]);
        for (int i = 0; i < cnt; i++) {
            strcpy(stack[++top], splited[i]);
            //printf("Push3: %s\n", stack[top]);
        }
        start = 1;
    } else {
        char* splited[MAXOPENFILE];
        int cnt = split_path(currentdir, splited); // 分割当前目录路径
        strcpy(stack[++top], "/");
        //printf("Push4: %s\n", stack[top]);
        for (int i = 0; i < cnt; i++) {
            strcpy(stack[++top], splited[i]);
            //printf("Push5: %s\n", stack[top]);
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
            //printf("Push6: %s\n", stack[top]);
        }
    }
    //printf("Origin Res : %s\n", res);
    for (int i = 0; i <= top; i++) {
        //printf("Stack[%d]: %s\n", i, stack[i]);
        if (i == 0 || i == 1) {
            strcat(res, stack[i]);
            //printf("Res1: %s Cnt: %d\n", res, i);
        } else {
            strcat(res, "/");
            strcat(res, stack[i]);
            //printf("Res2: %s Cnt: %d\n", res, i);
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
        // 如果是根目录，则不能再向上
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
    //printf("Parsed path: %s\n", parsed_path);

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

// 显示目录内容函数
void my_ls() {
    useropen* current = &openfilelist[curdir];
    unsigned short block = current->first;
    int file_count = 0;
    int dir_count = 0;
    long total_size = 0;

    printf("\nDirectory of %s\n\n", currentdir);
    printf("%-12s %-4s %10s %12s %s\n", "Name", "Type", "Size", "Date", "Time");

    // 遍历当前目录的所有块
    while (block != END) {
        fcb* dir_block = (fcb*)(myvhard + block * BLOCKSIZE);

        // 遍历块中的所有FCB
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++) {
            if (dir_block[i].free == 1) {  // 有效的FCB
                // 跳过 "." 和 ".." 目录项
                if (strcmp(dir_block[i].filename, ".") == 0 ||
                    strcmp(dir_block[i].filename, "..") == 0) {
                    continue;
                }

                // 解析日期和时间
                unsigned short date = dir_block[i].date;
                unsigned short time = dir_block[i].time;

                int year = (date >> 9) + 1980;
                int month = (date >> 5) & 0x0F;
                int day = date & 0x1F;

                int hour = time >> 11;
                int minute = (time >> 5) & 0x3F;
                int second = (time & 0x1F) * 2;

                // 显示文件/目录信息
                if (dir_block[i].attribute & 0x10) { // 目录
                    printf("%-12s %-4s %10s %04d-%02d-%02d %02d:%02d:%02d\n",
                           dir_block[i].filename,
                           "<DIR>",
                           "",
                           year, month, day,
                           hour, minute, second);
                    dir_count++;
                } else { // 文件
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
        block = fat1[block].id; // 获取下一个块
    }

    // 显示统计信息
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

/**
 * @brief 创建目录函数
 * @param dirname 目录名
 * @attention 默认在当前目录下创建新目录，即子目录
 */
void my_mkdir(char* dirname) {
    // 检查目录名长度
    if (strlen(dirname) > 8) {
        printf("Directory name too long (max 8 characters)\n");
        return;
    }
    // 当前所在的目录文件
    useropen* current = &openfilelist[curdir];

    // 检查是否重名
    fcb* existing = NULL;
    if (find_fcb(current, dirname, &existing) == 0) {
        printf("Directory already exists: %s\n", dirname);
        return;
    }

    // 寻找空闲FCB槽位
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
        // 需要分配新的目录块
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

        // 更新FAT表
        fat1[new_block].id = END;
        fat2[new_block].id = END;

        // 链接到当前目录
        block = current->first;
        while (fat1[block].id != END) {
            block = fat1[block].id;
        }
        fat1[block].id = new_block;
        fat2[block].id = new_block;

        // 初始化新块
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

    // 初始化新目录
    fat1[new_dir_block].id = END;
    fat2[new_dir_block].id = END;
    fcb* new_dir = (fcb*)(myvhard + new_dir_block * BLOCKSIZE);
    memset(new_dir, 0, BLOCKSIZE);

    // 创建 . 和 .. 目录项
    strcpy(new_dir[0].filename, ".");
    new_dir[0].attribute = 0x10; // 目录属性
    new_dir[0].first = new_dir_block;
    new_dir[0].length = BLOCKSIZE;
    new_dir[0].free = 1;

    strcpy(new_dir[1].filename, "..");
    new_dir[1].attribute = 0x10;
    new_dir[1].first = current->first;
    new_dir[1].length = current->length;
    new_dir[1].free = 1;

    // 设置当前时间
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    unsigned short date = ((tm_now->tm_year - 80) << 9) |
        ((tm_now->tm_mon + 1) << 5) |
        tm_now->tm_mday;
    unsigned short time = (tm_now->tm_hour << 11) |
        (tm_now->tm_min << 5) |
        (tm_now->tm_sec / 2);

    // 创建新目录的FCB
    strcpy(free_fcb->filename, dirname);
    free_fcb->attribute = 0x10; // 目录属性
    free_fcb->first = new_dir_block;
    free_fcb->length = BLOCKSIZE;
    free_fcb->time = time;
    free_fcb->date = date;
    free_fcb->free = 1;

    // 标记父目录为已修改
    current->fcbstate = 1;

    printf("Directory created: %s\n", dirname);
}

// 删除子目录函数
void my_rmdir(char* dirname) {
    useropen* current = &openfilelist[curdir];

    // 查找要删除的目录
    fcb* target_fcb = NULL;
    if (find_fcb(current, dirname, &target_fcb) != 0) {
        printf("Directory not found: %s\n", dirname);
        return;
    }

    // 检查是否是目录
    if (!(target_fcb->attribute & 0x10)) {
        printf("Not a directory: %s\n", dirname);
        return;
    }

    // 检查目录是否为空
    useropen temp_dir;
    strcpy(temp_dir.filename, dirname);
    temp_dir.first = target_fcb->first;
    temp_dir.length = target_fcb->length;

    if (!is_dir_empty(&temp_dir)) {
        printf("Directory not empty: %s\n", dirname);
        return;
    }

    // 回收目录块
    unsigned short block = target_fcb->first;
    while (block != END) {
        unsigned short next = fat1[block].id;
        fat1[block].id = FREE;
        fat2[block].id = FREE;
        block = next;
    }

    // 从父目录中删除FCB
    target_fcb->free = 0;

    // 标记父目录为已修改
    current->fcbstate = 1;

    printf("Directory removed: %s\n", dirname);
}

int my_create(char* filename) {
    // 检查文件名长度
    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen* current = &openfilelist[curdir];

    // 检查是否重名
    fcb* existing = NULL;
    if (find_fcb(current, filename, &existing) == 0) {
        printf("File already exists: %s\n", filename);
        return -1;
    }

    // 寻找空闲FCB槽位
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
        // 需要分配新的目录块
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

        // 更新FAT表
        fat1[new_block].id = END;
        fat2[new_block].id = END;

        // 链接到当前目录
        block = current->first;
        while (fat1[block].id != END) {
            block = fat1[block].id;
        }
        fat1[block].id = new_block;
        fat2[block].id = new_block;

        // 初始化新块
        fcb* new_dir_block = (fcb*)(myvhard + new_block * BLOCKSIZE);
        memset(new_dir_block, 0, BLOCKSIZE);
        free_fcb = &new_dir_block[0];
    }

    // 分配新文件的块
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

    // 初始化新文件块
    fat1[new_file_block].id = END;
    fat2[new_file_block].id = END;
    memset(myvhard + new_file_block * BLOCKSIZE, 0, BLOCKSIZE);

    // 设置当前时间
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    unsigned short date = ((tm_now->tm_year - 80) << 9) |
        ((tm_now->tm_mon + 1) << 5) |
        tm_now->tm_mday;
    unsigned short time = (tm_now->tm_hour << 11) |
        (tm_now->tm_min << 5) |
        (tm_now->tm_sec / 2);

    // 创建新文件的FCB
    strcpy(free_fcb->filename, filename);
    free_fcb->attribute = 0x00; // 文件属性
    free_fcb->first = new_file_block;
    free_fcb->length = 0;
    free_fcb->time = time;
    free_fcb->date = date;
    free_fcb->free = 1;

    // 在用户打开文件表中分配表项
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

    // 标记父目录为已修改
    current->fcbstate = 1;

    printf("File created: %s (fd=%d)\n", filename, fd);
    return fd;
}


void my_rm(char* filename) {
    useropen* current = &openfilelist[curdir];

    // 查找要删除的文件
    fcb* target_fcb = NULL;
    if (find_fcb(current, filename, &target_fcb) != 0) {
        printf("File not found: %s\n", filename);
        return;
    }

    // 检查是否是文件
    if (target_fcb->attribute & 0x10) {
        printf("Not a regular file: %s (use my_rmdir for directories)\n", filename);
        return;
    }

    // 检查文件是否被打开
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile &&
            openfilelist[i].first == target_fcb->first) {
            printf("File is currently open (fd=%d), close it first\n", i);
            return;
        }
    }

    // 回收文件块
    unsigned short block = target_fcb->first;
    while (block != END) {
        unsigned short next = fat1[block].id;
        fat1[block].id = FREE;
        fat2[block].id = FREE;
        block = next;
    }

    // 从父目录中删除FCB
    target_fcb->free = 0;

    // 标记父目录为已修改
    current->fcbstate = 1;

    printf("File removed: %s\n", filename);
}

int my_open(char* filename) {
    // 检查文件名长度
    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen* current = &openfilelist[curdir];

    // 查找文件FCB
    fcb* file_fcb = NULL;
    if (find_fcb(current, filename, &file_fcb) != 0) {
        printf("File not found: %s\n", filename);
        return -1;
    }

    // 检查是否是文件
    if (file_fcb->attribute & 0x10) {
        printf("%s is a directory (use my_cd)\n", filename);
        return -1;
    }

    // 检查是否已经打开
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile &&
            openfilelist[i].first == file_fcb->first &&
            strcmp(openfilelist[i].dir, currentdir) == 0) {
            printf("File already opened (fd=%d)\n", i);
            return i;
        }
    }

    // 在用户打开文件表中分配表项
    int fd = -1;
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (!openfilelist[i].topenfile) {
            // 初始化打开文件表项
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
    // 检查文件名有效性
    if (strlen(filename) > 8) {
        printf("Invalid filename: %s\n", filename);
        return;
    }

    // 在打开文件表中查找匹配项
    int close_count = 0;
    int current_dir_len = strlen(currentdir);

    for (int fd = 0; fd < MAXOPENFILE; fd++) {
        if (openfilelist[fd].topenfile &&
            strcmp(openfilelist[fd].filename, filename) == 0 &&
            strncmp(openfilelist[fd].dir, currentdir, current_dir_len) == 0) {

            // 检查FCB是否需要更新
            if (openfilelist[fd].fcbstate) {
                // 查找父目录
                useropen* parent = NULL;
                for (int i = 0; i < MAXOPENFILE; i++) {
                    if (openfilelist[i].topenfile &&
                        strcmp(openfilelist[i].dir, openfilelist[fd].dir) == 0) {
                        parent = &openfilelist[i];
                        break;
                    }
                }

                if (parent) {
                    // 查找对应的FCB
                    fcb* file_fcb = NULL;
                    if (find_fcb(parent, filename, &file_fcb) == 0) {
                        // 同步所有需要更新的属性
                        file_fcb->length = openfilelist[fd].length;
                        file_fcb->time = openfilelist[fd].time;
                        file_fcb->date = openfilelist[fd].date;

                        // 标记父目录为已修改
                        parent->fcbstate = 1;
                    }
                }
            }

            // 清空打开文件表项
            memset(&openfilelist[fd], 0, sizeof(useropen));
            openfilelist[fd].topenfile = 0;
            close_count++;
        }
    }

    // 处理结果反馈
    if (close_count > 0) {
        printf("Closed %d instance(s) of %s\n", close_count, filename);
    } else {
        printf("File not open: %s\n", filename);
    }
}

int is_file_open(char* filename) {
    int current_dir_len = strlen(currentdir);
    for (int i = 0; i < MAXOPENFILE; i++) {
        if (openfilelist[i].topenfile &&
            strcmp(openfilelist[i].filename, filename) == 0 &&
            strncmp(openfilelist[i].dir, currentdir, current_dir_len) == 0) {
            return 1;
        }
    }
    return 0;
}

int do_write(int fd, char* text, int len, char wstyle);

int my_write(int fd) {
    // 检查文件描述符有效性
    if (fd < 0 || fd >= MAXOPENFILE || !openfilelist[fd].topenfile) {
        printf("Invalid file descriptor: %d\n", fd);
        return -1;
    }

    // 检查文件属性
    if (openfilelist[fd].attribute & 0x10) {
        printf("Cannot write to a directory\n");
        return -1;
    }

    // 选择写入方式
    printf("Select write style:\n");
    printf("1. Truncate write (overwrite entire file)\n");
    printf("2. Overwrite write (write from current position)\n");
    printf("3. Append write (write at end of file)\n");
    printf("Your choice (1-3): ");

    char wstyle;
    scanf("%hhd", &wstyle);
    getchar(); // 消耗换行符

    if (wstyle < 1 || wstyle > 3) {
        printf("Invalid write style selection\n");
        return -1;
    }

    // 设置文件指针位置
    switch (wstyle) {
        case 1: // 截断写
            openfilelist[fd].count = 0;
            openfilelist[fd].length = 0;
            break;
        case 2: // 覆盖写
            // 保持当前count位置不变
            break;
        case 3: // 追加写
            openfilelist[fd].count = openfilelist[fd].length;
            break;
    }

    // 读取用户输入
    printf("Enter content (end with Ctrl+Z):\n");

    char buffer[BLOCK_SIZE];
    char input_buffer[BLOCK_SIZE * 4]; // 临时存储区，可容纳4个块的内容
    int input_len = 0;
    char ch;

    while ((ch = getchar()) != CTRL_Z && input_len < sizeof(input_buffer) - 1) {
        input_buffer[input_len++] = ch;
    }
    input_buffer[input_len] = '\0';

    // 调用实际写入函数
    int write_len = do_write(fd, input_buffer, input_len, wstyle);
    if (write_len < 0) {
        printf("Write failed\n");
        return -1;
    }

    // 更新文件长度（如果是追加或截断写）
    if (wstyle == 1 || wstyle == 3) {
        openfilelist[fd].length += write_len;
    }

    // 标记文件为已修改
    openfilelist[fd].fcbstate = 1;

    printf("Successfully wrote %d bytes to file (fd=%d)\n", write_len, fd);
    return write_len;
}

int do_write(int fd, char* text, int len, char wstyle) {
    useropen* file = &openfilelist[fd];
    int bytes_written = 0;
    int remaining = len;
    char* ptr = text;

    // 计算起始位置
    unsigned int logical_block = file->count / BLOCK_SIZE;
    unsigned int block_offset = file->count % BLOCK_SIZE;

    // 遍历找到起始块
    unsigned short current_block = file->first;
    for (unsigned int i = 0; i < logical_block && current_block != END; i++) {
        current_block = fat1[current_block].id;
    }

    if (current_block == END && logical_block > 0) {
        printf("Error: Invalid file position\n");
        return -1;
    }

    // 如果是截断写且需要新块，先释放原有块
    if (wstyle == 1) {
        unsigned short block = file->first;
        while (block != END) {
            unsigned short next = fat1[block].id;
            fat1[block].id = FREE;
            fat2[block].id = FREE;
            block = next;
        }
        file->first = END;
        current_block = END;
        file->count = 0;
        block_offset = 0;
    }

    // 写入循环
    while (remaining > 0) {
        // 需要新块的情况
        if (current_block == END || block_offset == 0) {
            // 分配新块
            unsigned short new_block = 0;
            for (int i = ROOTBLOCKNUM + 1; i < SIZE / BLOCKSIZE; i++) {
                if (fat1[i].id == FREE) {
                    new_block = i;
                    break;
                }
            }

            if (new_block == 0) {
                printf("Error: No free space available\n");
                break;
            }

            // 链接新块
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

            // 初始化新块
            memset(myvhard + current_block * BLOCK_SIZE, 0, BLOCK_SIZE);
        }

        // 计算本次写入长度
        int write_size = BLOCK_SIZE - block_offset;
        if (write_size > remaining) {
            write_size = remaining;
        }

        // 执行写入
        char* block_ptr = (char*)(myvhard + current_block * BLOCK_SIZE + block_offset);
        memcpy(block_ptr, ptr, write_size);

        // 更新指针和计数器
        ptr += write_size;
        remaining -= write_size;
        bytes_written += write_size;
        file->count += write_size;
        block_offset += write_size;

        // 如果块写满，移动到下一个块
        if (block_offset >= BLOCK_SIZE) {
            current_block = fat1[current_block].id;
            block_offset = 0;
        }
    }

    // 更新文件长度（如果需要）
    if (file->count > file->length) {
        file->length = file->count;
    }

    return bytes_written;
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
        } else if (strcmp(cmd, "pwd") == 0) {
            printf("Current directory: %s\n", currentdir);
        } else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}