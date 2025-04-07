
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCKSIZE 1024
#define SIZE 1024000
#define END 65535
#define FREE 0 
#define ROOTBLOCKNUM 2
#define MAXOPENFILE 10

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
    unsigned char *startblock;
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
 unsigned char *myvhard;
 useropen openfilelist[MAXOPENFILE];
 int curdir;
 char currentdir[80];
 unsigned char *startp;
 fat *fat1, *fat2;
 block0 *bootblock;

void my_format();
void my_exitsys();
void my_cd(char *dirname);

void startsys() {
    myvhard = (unsigned char *)malloc(SIZE);
    if (!myvhard) {
        printf("Failed to allocate memory for virtual disk.\n");
        exit(1);
    }

    FILE *fp = fopen("filesys", "rb");
    if (fp) {
        fread(myvhard, SIZE, 1, fp);
        fclose(fp);
    } else {
        my_format();
    }

    // 初始化全局指针
    bootblock = (block0 *)myvhard;
    fat1 = (fat *)(myvhard + BLOCKSIZE);          // FAT1起始块
    fat2 = (fat *)(myvhard + 3 * BLOCKSIZE);      // FAT2起始块（FAT1占2块）

    // 初始化用户打开文件表项0（根目录）
    fcb *root_fcb = (fcb *)(myvhard + bootblock->root * BLOCKSIZE);
    useropen *root = &openfilelist[0];
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
    for (int i = 1; i < MAXOPENFILE; i++) {
        openfilelist[i].topenfile = 0;
    }
}

// 格式化虚拟磁盘
void my_format() {
    if (!myvhard) {
        myvhard = (unsigned char *)malloc(SIZE);
    }

    // 初始化引导块
    bootblock = (block0 *)myvhard;
    strcpy(bootblock->information, "Simple File System");
    bootblock->root = ROOTBLOCKNUM;                    // 根目录起始块号
    bootblock->startblock = myvhard + 7 * BLOCKSIZE;   // 数据区起始地址

    // 初始化FAT表
    int total_blocks = SIZE / BLOCKSIZE;
    fat1 = (fat *)(myvhard + BLOCKSIZE);
    fat2 = (fat *)(myvhard + 3 * BLOCKSIZE);

    // 标记系统使用的块
    for (int i = 0; i < total_blocks; i++) {
        if (i == 0 || (i >= 1 && i <= 4) || i == ROOTBLOCKNUM || i == ROOTBLOCKNUM + 1) {
            fat1[i].id = END;
            fat2[i].id = END;
        } else {
            fat1[i].id = FREE;
            fat2[i].id = FREE;
        }
    }

    // 初始化根目录区（块5和块6）
    fcb *root_dir = (fcb *)(myvhard + ROOTBLOCKNUM * BLOCKSIZE);
    memset(root_dir, 0, 2 * BLOCKSIZE);

    // 创建根目录的.和..条目
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

    // 设置用户打开文件表项0
    useropen *root = &openfilelist[0];
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
        FILE *fp = fopen("filesys", "wb");
        if (fp) {
            fwrite(myvhard, SIZE, 1, fp);
            fclose(fp);
        }
        free(myvhard);
        myvhard = NULL;
    }
}


int split_path(char *path, char **parts) {
    int count = 0;
    char *token = strtok(path, "/");
    while (token != NULL && count < MAXOPENFILE) {
        parts[count++] = token;
        token = strtok(NULL, "/");
    }
    return count;
}


int find_fcb(useropen *dir, char *name, fcb **found) {
    unsigned short block = dir->first;
    
    while (block != END) {
        fcb *dir_block = (fcb *)(myvhard + block * BLOCKSIZE);
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

void my_cd(char *dirname) {
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
        if (parent_dir[strlen(parent_dir)-1] == '/') {
            parent_dir[strlen(parent_dir)-1] = '\0';
        }
        
        // 找到上一级目录
        char *last_slash = strrchr(parent_dir, '/');
        if (last_slash != NULL) {
            *last_slash = '\0';
            if (strlen(parent_dir) == 0) {
                strcpy(parent_dir, "/"); // 回到根目录
            } else {
                strcat(parent_dir, "/"); // 确保以/结尾
            }
        }
        
        // 查找父目录FCB
        fcb *parent_fcb = NULL;
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


    // 处理路径中的斜杠
    char normalized[80];
    strcpy(normalized, dirname);
    if (normalized[strlen(normalized)-1] == '/') {
        normalized[strlen(normalized)-1] = '\0';
    }

    // 查找目录
    fcb *target_fcb = NULL;

    
    if (find_fcb(&openfilelist[curdir], normalized, &target_fcb) != 0) {
        printf("Error: Directory '%s' not found in %s\n", normalized, currentdir);
        return;
    }

    // 检查是否是目录
    if (!(target_fcb->attribute & 0x10)) {
        printf("Error: '%s' is not a directory\n", normalized);
        return;
    }

    // 更新当前目录
    char new_dir[80];
    if (normalized[0] == '/') {
        // 绝对路径
        strcpy(new_dir, normalized);
    } else {
        // 相对路径
        strcpy(new_dir, currentdir);
        if (currentdir[strlen(currentdir)-1] != '/') {
            strcat(new_dir, "/");
        }
        strcat(new_dir, normalized);
    }
    
    // 确保路径以/结尾
    if (new_dir[strlen(new_dir)-1] != '/') {
        strcat(new_dir, "/");
    }

    // 查找是否已经打开
    int found = -1;
    for (int j = 0; j < MAXOPENFILE; j++) {
        if (openfilelist[j].topenfile && 
            openfilelist[j].first == target_fcb->first) {
            found = j;
            break;
        }
    }

    // 如果未打开则分配新表项
    if (found == -1) {
        for (int j = 0; j < MAXOPENFILE; j++) {
            if (!openfilelist[j].topenfile) {
                strcpy(openfilelist[j].filename, target_fcb->filename);
                openfilelist[j].attribute = target_fcb->attribute;
                openfilelist[j].first = target_fcb->first;
                openfilelist[j].length = target_fcb->length;
                strcpy(openfilelist[j].dir, new_dir);
                openfilelist[j].topenfile = 1;
                found = j;
                break;
            }
        }
        if (found == -1) {
            printf("Error: Too many open files!\n");
            return;
        }
    }

    // 更新当前目录
    curdir = found;
    strcpy(currentdir, new_dir);
    printf("Current directory changed to: %s\n", currentdir);
}

// 显示目录内容函数
void my_ls() {
    useropen *current = &openfilelist[curdir];
    unsigned short block = current->first;
    int file_count = 0;
    int dir_count = 0;
    long total_size = 0;

    printf("\nDirectory of %s\n\n", currentdir);
    printf("%-12s %-4s %10s %12s %s\n", "Name", "Type", "Size", "Date", "Time");

    // 遍历当前目录的所有块
    while (block != END) {
        fcb *dir_block = (fcb *)(myvhard + block * BLOCKSIZE);
        
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
bool is_dir_empty(useropen *dir) {
    unsigned short block = dir->first;
    int entry_count = 0;

    while (block != END) {
        fcb *dir_block = (fcb *)(myvhard + block * BLOCKSIZE);
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

// 创建子目录函数
void my_mkdir(char *dirname) {
    // 检查目录名长度
    if (strlen(dirname) > 8) {
        printf("Directory name too long (max 8 characters)\n");
        return;
    }

    useropen *current = &openfilelist[curdir];
    
    // 检查是否重名
    fcb *existing = NULL;
    if (find_fcb(current, dirname, &existing) == 0) {
        printf("Directory already exists: %s\n", dirname);
        return;
    }

    // 寻找空闲FCB槽位
    fcb *free_fcb = NULL;
    unsigned short block = current->first;
    int found = 0;
    
    while (block != END && !found) {
        fcb *dir_block = (fcb *)(myvhard + block * BLOCKSIZE);
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
        for (int i = 5; i < SIZE/BLOCKSIZE; i++) {
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
        fcb *new_dir_block = (fcb *)(myvhard + new_block * BLOCKSIZE);
        memset(new_dir_block, 0, BLOCKSIZE);
        free_fcb = &new_dir_block[0];
    }

    // 分配新目录的空间
    unsigned short new_dir_block = 0;
    for (int i = 5; i < SIZE/BLOCKSIZE; i++) {
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
    fcb *new_dir = (fcb *)(myvhard + new_dir_block * BLOCKSIZE);
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
    struct tm *tm_now = localtime(&now);
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
void my_rmdir(char *dirname) {
    useropen *current = &openfilelist[curdir];
    
    // 查找要删除的目录
    fcb *target_fcb = NULL;
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

int my_create(char *filename) {
    // 检查文件名长度
    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen *current = &openfilelist[curdir];
    
    // 检查是否重名
    fcb *existing = NULL;
    if (find_fcb(current, filename, &existing) == 0) {
        printf("File already exists: %s\n", filename);
        return -1;
    }

    // 寻找空闲FCB槽位
    fcb *free_fcb = NULL;
    unsigned short block = current->first;
    int found = 0;
    
    while (block != END && !found) {
        fcb *dir_block = (fcb *)(myvhard + block * BLOCKSIZE);
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
        for (int i = ROOTBLOCKNUM + 1; i < SIZE/BLOCKSIZE; i++) {
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
        fcb *new_dir_block = (fcb *)(myvhard + new_block * BLOCKSIZE);
        memset(new_dir_block, 0, BLOCKSIZE);
        free_fcb = &new_dir_block[0];
    }

    // 分配新文件的块
    unsigned short new_file_block = 0;
    for (int i = ROOTBLOCKNUM + 1; i < SIZE/BLOCKSIZE; i++) {
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
    struct tm *tm_now = localtime(&now);
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


void my_rm(char *filename) {
    useropen *current = &openfilelist[curdir];
    
    // 查找要删除的文件
    fcb *target_fcb = NULL;
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

int my_open(char *filename) {
    // 检查文件名长度
    if (strlen(filename) > 8) {
        printf("Filename too long (max 8 characters)\n");
        return -1;
    }

    useropen *current = &openfilelist[curdir];
    
    // 查找文件FCB
    fcb *file_fcb = NULL;
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

void my_close(int fd) {
    // 检查文件描述符有效性
    if (fd < 0 || fd >= MAXOPENFILE) {
        printf("Invalid file descriptor: %d\n", fd);
        return;
    }

    if (!openfilelist[fd].topenfile) {
        printf("File descriptor not in use: %d\n", fd);
        return;
    }

    // 如果需要，更新FCB信息
    if (openfilelist[fd].fcbstate) {
        // 查找父目录
        useropen *parent = NULL;
        for (int i = 0; i < MAXOPENFILE; i++) {
            if (openfilelist[i].topenfile && 
                strcmp(openfilelist[i].dir, openfilelist[fd].dir) == 0) {
                parent = &openfilelist[i];
                break;
            }
        }

        if (parent) {
            // 查找对应的FCB
            fcb *file_fcb = NULL;
            if (find_fcb(parent, openfilelist[fd].filename, &file_fcb) == 0) {
                // 更新FCB信息
                file_fcb->length = openfilelist[fd].length;
                // 可以添加其他需要同步的属性
                
                // 标记父目录为已修改
                parent->fcbstate = 1;
            }
        }
    }

    // 清空打开文件表项
    memset(&openfilelist[fd], 0, sizeof(useropen));
    openfilelist[fd].topenfile = 0;

    printf("File closed (fd=%d)", fd);
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
        if (strcmp(cmd, "my_format") == 0) {
            my_format();
            printf("File system formatted.\n");
        } 
        else if (strcmp(cmd, "my_open") == 0) {
            scanf("%s", arg);
            fd = my_open(arg);
        }
        else if (strcmp(cmd, "my_close") == 0) {
            scanf("%d", &fd);
            my_close(fd);
        }
        else if (strcmp(cmd, "my_exitsys") == 0) {
            my_exitsys();
            printf("Exiting system.\n");
            break;
        
        } 
        else if (strcmp(cmd, "my_create") == 0) {
            scanf("%s", arg);
            my_create(arg);
        }
        else if (strcmp(cmd, "my_rm") == 0) {
            scanf("%s", arg);
            my_rm(arg);
        }
        else if (strcmp(cmd, "my_mkdir") == 0) {
           
            scanf("%s", path);
            my_mkdir(path);
        } else if (strcmp(cmd, "my_rmdir") == 0) {
            scanf("%s", path);
            my_rmdir(path);
        }
        else if(strcmp(cmd, "my_ls") == 0){
            my_ls();
        }
        else if (strcmp(cmd, "my_cd") == 0) {
            scanf("%s", path);
            // printf("%s",path);
            my_cd(path);
        }
        else {
            printf("Unknown command.\n");
        }
    }

    return 0;
}