//
//  yfs.h
//  yfs
//
//  Created by Wenzhe Jiang on 4/22/15.
//  Copyright (c) 2015 Wenzhe Jiang. All rights reserved.
//

#ifndef yfs_yfs_h
#define yfs_yfs_h
#include <stdio.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdlib.h>
#include <string.h>

#define hash_size 97

#define OPEN 0
#define CLOSE 1
#define CREATE 2
#define READ 3
#define WRITE 4
#define SEEK 5
#define LINK 6
#define UNLINK 7
#define SYMLINK 8
#define READLINK 9
#define MKDIR 10
#define RMDIR 11
#define CHDIR 12
#define STAT 13
#define SYNC 14
#define SHUTDOWN 15

typedef struct Msg {
    int type;
    int num1;
    int num2;
    int num3;
    void *ptr1;
    void *ptr2;
} Msg;

typedef struct block_cache {
    int dirty;
    int block_num;
    struct block_cache *prev;
    struct block_cache *next;
    char data[BLOCKSIZE];
} block_cache;

typedef struct inode_cache {
    int dirty;
    int inode_num;
    struct inode_cache *prev;
    struct inode_cache *next;
    struct inode data;
} inode_cache;

typedef struct block_hash {
    int key;
    block_cache *value;
    struct block_hash *next;
} block_hash;

typedef struct inode_hash{
    int key;
    inode_cache *value;
    struct inode_hash *next;
} inode_hash;

int inode_to_block(int);

void put_block(int,block_cache*);

block_cache* get_block(int);

void rm_block(int block_num);

block_cache* get_blockLRU(int);

void set_blockLRU(int, block_cache*);

block_cache* read_block(int);

block_cache* init_block_cache(int);

void put_inode(int, inode_cache *);

inode_cache* get_inode(int);

void rm_inode(int);

inode_cache* get_inodeLRU(int);

void set_inodeLRU(int, inode_cache*);

inode_cache* read_inode(int);

void check_used_blocks_inodes(int);

void initialize(void);

void terminate(void);

int path_to_inum(char *pathname, int len_path, int proc_inum, int symlink_cnt);

void open_handler(Msg *msg, int sender_pid);

int isEqual(char* s1, char* s2);

int check_dir(int direct_inum, char* filename);

struct dir_entry *search_dir_entry(int direct_inum, char* filename);

void free_inode(int inum);

int alloc_inode(int type, int pare_inum);

struct dir_entry *empty_dir(int dir_inum);

void create_handler(Msg *msg, int sender_pid);

void read_handler(Msg *msg, int sender_pid);

int alloc_block();

void write_handler(Msg *msg, int sender_pid);

void mkdir_handler(Msg *msg, int sender_pid);

void rmdir_handler(Msg *msg, int sender_pid);

void chdir_handler(Msg *msg, int sender_pid);

void link_handler(Msg *msg, int sender_pid);

void unlink_handler(Msg *msg, int sender_pid);

void symlink_handler(Msg *msg, int sender_pid);

void readlink_handler(Msg *msg, int sender_pid);

void stat_handler(Msg *msg, int sender_pid);

void sync_handler(Msg *msg);

void shutdown_handler(Msg *msg, int sender_pid);

/* debug functions */
void print_dir_entries(int dir_inum);

#endif
