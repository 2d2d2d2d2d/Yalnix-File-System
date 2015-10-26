//
//  yfs.c
//  yfs
//
//  Created by Wenzhe Jiang & Jun Liu on 4/19/15.
//  Copyright (c) 2015 Wenzhe Jiang & Jun Liu. All rights reserved.
//
#include "yfs.h"

static const int INODENUM = BLOCKSIZE/INODESIZE;
static const int DIRS_PER_BLOCK = BLOCKSIZE/sizeof(struct dir_entry);

int total_blocks, total_inodes;

//indicate if inode or block is free ('y') or not free ('n')
char *inodes_isfree;
char *blocks_isfree;

int block_cache_cnt = 0;

block_cache *first_block, *last_block;

int inode_cache_cnt = 0;

inode_cache *first_inode, *last_inode;

block_hash* block_st[hash_size];

inode_hash* inode_st[hash_size];

int sync();

//return the block num for inode
int inode_to_block(int inode_num)
{
    return 1+(inode_num/(BLOCKSIZE/INODESIZE));
}

//put a block into hashtable
void put_block(int block_num, block_cache *block)
{
    int idx = block_num%hash_size;
    block_hash *tmp = block_st[idx];
    while (tmp!=NULL) {
        if (tmp->key==block_num) {
            free(tmp->value);
            tmp->value = block;
            return;
        }
        tmp = tmp->next;
    }
    tmp = block_st[idx];
    block_st[idx] = (block_hash*)malloc(sizeof(block_hash));
    block_st[idx]->key = block_num;
    block_st[idx]->value = block;
    block_st[idx]->next = tmp;
}

//get a block from hash
block_cache* get_block(int block_num)
{
    int idx = block_num%hash_size;
    block_hash *tmp = block_st[idx];
    while (tmp!=NULL) {
        if (tmp->key==block_num) return tmp->value;
        tmp = tmp->next;
    }
    return NULL;
}

//remove a block from hash
void rm_block(int block_num)
{
    int idx = block_num%hash_size;
    block_hash *tmp = block_st[idx];
    if (tmp==NULL) return;
    if (tmp->key==block_num) {
        block_st[idx] = tmp->next;
        free(tmp);
        return;
    }
    while (tmp->next!=NULL) {
        if (tmp->next->key==block_num) {
            block_hash *t = tmp->next;
            tmp->next = t->next;
            free(t);
            return;
        }
        tmp = tmp->next;
    }
}

//get a block LRU cache
block_cache* get_blockLRU(int block_num)
{
    block_cache* ret = get_block(block_num);
    if (ret==NULL) {
        return NULL;
    }
    else if (ret!=last_block){
        if (ret == first_block) first_block = first_block->next;
        else ret->prev->next = ret->next;
        ret->next->prev = ret->prev;
        last_block->next = ret;
        ret->prev = last_block;
        ret->next = NULL;
        last_block = ret;
    }
    return ret;
}

//insert or update a block in LRU cache
void set_blockLRU(int block_num, block_cache* b)
{
    block_cache* n = get_block(block_num);
    if (n!=NULL) {
        put_block(block_num, b);
        if (n!=last_block) {
            if (n==first_block) first_block = first_block->next;
            else n->prev->next = n->next;
            n->next->prev = n->prev;
            last_block->next = n;
            n->prev = last_block;
            n->next = NULL;
            last_block = n;
        }
    }
    else {
        if (block_cache_cnt>=BLOCK_CACHESIZE) {
            sync();
            rm_block(first_block->block_num);

            //write back dirty block to disk
            if (first_block->dirty) {
                if (WriteSector(first_block->block_num,(void*)(first_block->data))!=0) perror("erro writing back dirty block!");
            }
            block_cache* tmp = first_block;
            first_block = first_block->next;
            if (first_block!=NULL) first_block->prev = NULL;
            else last_block = NULL;
            block_cache_cnt--;
            free(tmp);
        }
        if (first_block==NULL || last_block==NULL) first_block = b;
        else last_block->next = b;
        b->prev = last_block;
        last_block = b;
        put_block(block_num, b);
        block_cache_cnt++;
    }
}

//put an inode to hash
void put_inode(int inode_num, inode_cache *i)
{
    int idx = inode_num%hash_size;
    inode_hash *tmp = inode_st[idx];
    while (tmp!=NULL) {
        if (tmp->key==inode_num) {
            free(tmp->value);
            tmp->value = i;
            return;
        }
        tmp = tmp->next;
    }
    tmp = inode_st[idx];
    inode_st[idx] = (inode_hash*)malloc(sizeof(inode_hash));
    inode_st[idx]->key = inode_num;
    inode_st[idx]->value = i;
    inode_st[idx]->next = tmp;
}

//get an inode from hash
inode_cache* get_inode(int inode_num)
{
    int idx = inode_num%hash_size;
    inode_hash *tmp = inode_st[idx];
    while (tmp!=NULL) {
        if (tmp->key==inode_num){
            
            return tmp->value;
        }
        tmp = tmp->next;
    }
    return NULL;
}

//remove an inode from hash
void rm_inode(int inode_num)
{
    int idx = inode_num%hash_size;
    inode_hash *tmp = inode_st[idx];
    if (tmp==NULL) return;
    if (tmp->key==inode_num) {
        inode_st[idx] = tmp->next;
        free(tmp);
        return;
    }
    while (tmp->next!=NULL) {
        if (tmp->next->key==inode_num) {
            inode_hash *t = tmp->next;
            tmp->next = t->next;
            free(t);
            return;
        }
        tmp = tmp->next;
    }
}

//get an inode from inode cache LRU
inode_cache* get_inodeLRU(int inode_num)
{
    inode_cache* ret = get_inode(inode_num);
    if (ret==NULL) {
        return NULL;
    }
    else if (ret!=last_inode){
        if (ret == first_inode) first_inode = first_inode->next;
        else ret->prev->next = ret->next;
        ret->next->prev = ret->prev;
        last_inode->next = ret;
        ret->prev = last_inode;
        ret->next = NULL;
        last_inode = ret;
    }
    return ret;
}

//insert and uodate an inode to inode cache LRU
void set_inodeLRU(int inode_num, inode_cache *i)
{
    inode_cache* n = get_inode(inode_num);
    if (n!=NULL) {
        put_inode(inode_num, i);
        if (n!=last_inode) {
            if (n==first_inode) first_inode = first_inode->next;
            else n->prev->next = n->next;
            n->next->prev = n->prev;
            last_inode->next = n;
            n->prev = last_inode;
            n->next = NULL;
            last_inode = n;
        }
    }
    else {
        if (inode_cache_cnt>=INODE_CACHESIZE) {
            //rm_inode(first_inode->inode_num);
            sync();
            //write back dirty inode to block cache
            if (first_inode->dirty) {
                int block_num = inode_to_block(inode_num);
                block_cache *b = read_block(block_num);
                int idx = inode_num-(block_num-1)*(BLOCKSIZE/INODESIZE);
                memcpy((void*)(b->data+idx*INODESIZE),(void*)(&(first_inode->data)),INODESIZE);
                b->dirty = 1;
            }
            rm_inode(first_inode->inode_num);
            inode_cache *tmp = first_inode;
            first_inode = first_inode->next;
            if (first_inode!=NULL) first_inode->prev = NULL;
            else last_inode = NULL;
            inode_cache_cnt--;
            free(tmp);
        }
        if (first_inode==NULL || last_inode==NULL) first_inode = i;
        else last_inode->next = i;
        i->prev = last_inode;
        last_inode = i;
        put_inode(inode_num, i);
        inode_cache_cnt++;
    }
}

//initialize block_cache
block_cache* init_block_cache(int block_num)
{
    block_cache *ret = (block_cache*)malloc(sizeof(block_cache));
    ret->block_num = block_num;
    ret->dirty = 0;
    ret->prev = NULL;
    ret->next = NULL;
    return ret;
}

//read a block
block_cache* read_block(int block_num)
{
    block_cache* ret = get_blockLRU(block_num);
    if (ret!=NULL) return ret;
    ret = init_block_cache(block_num);
    if (ReadSector(block_num, (void*)(ret->data))==ERROR) perror("read sector error!");
    ret->dirty = 0;
    set_blockLRU(block_num,ret);
    return ret;
}

//read an inode
inode_cache* read_inode(int inode_num)
{
    inode_cache* ret = get_inodeLRU(inode_num);
    if (ret!=NULL){
        return ret;
    }
    int block_num = inode_to_block(inode_num);
    block_cache *b = get_blockLRU(block_num);
    if (b==NULL) b = read_block(block_num);
    int idx = inode_num-(block_num-1)*(BLOCKSIZE/INODESIZE);
    ret = (inode_cache*)malloc(sizeof(inode_cache));
    ret->inode_num = inode_num;
    ret->dirty = 0;
    memcpy(&(ret->data),b->data+idx*INODESIZE,INODESIZE);
    set_inodeLRU(inode_num, ret);
    return ret;
}

//check used blocks and inodes
void check_used_blocks_inodes(int inode_num)
{

    inode_cache *cur = read_inode(inode_num);
    int block_num;


    if (cur->data.type==INODE_FREE) {
        return;
    }
    //remove self inode from inode_isfree list
    inodes_isfree[inode_num] = 'n';
    
    //remove used blocks of this inode from block_isfree list
    int i;
    for (i=0; i<NUM_DIRECT; i++) {
        if (cur->data.direct[i]!=0) {
            blocks_isfree[cur->data.direct[i]] = 'n';
        }
    }

    if (cur->data.indirect!=0) {
        blocks_isfree[cur->data.indirect] = 'n';
        block_num = cur->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                blocks_isfree[block_num] = 'n';
            }
            else break;
        }

    }

    //cur inode is a directory
    if (cur->data.type==INODE_DIRECTORY) {

        //check inodes on direct blocks
        for (i=0; i<NUM_DIRECT; i++) {
            block_num = cur->data.direct[i];
            if (block_num==0) break;

            block_cache *b = read_block(block_num);

            struct dir_entry *d = (struct dir_entry*)(b->data);
            int j;
            int start = (i==0?2:0);
            for (j=start; j<DIRS_PER_BLOCK; j++) {
                if (d[j].inum!=0) {
                    check_used_blocks_inodes(d[j].inum);
                }
            }
        }

        //check inodes on indirect blocks
        if (cur->data.indirect!=0) {
            block_num = cur->data.indirect;
            block_cache *b = read_block(block_num);
            int j;
            block_cache *tmp;
            for (j=0; j<BLOCKSIZE; j+=4) {
                block_num = *(int*)(b->data+j);
                if (block_num!=0) {
                    tmp = read_block(block_num);
                    struct dir_entry *d = (struct dir_entry*)(tmp->data);
                    int k;
                    for (k=0; k<DIRS_PER_BLOCK; k++) {
                        if (d[k].inum!=0) {
                            check_used_blocks_inodes(d[k].inum);
                        }
                    }
                }
                else break;
            }
        }
    }
}

//initialize free block list, free inodes list and global paras
void initialize(void)
{
    inode_cache *n=read_inode(0);
    struct fs_header *h = (struct fs_header*)(&(n->data));

    total_blocks = h->num_blocks;
    total_inodes = h->num_inodes;

    blocks_isfree = (char*)malloc(sizeof(char)*total_blocks);
    inodes_isfree = (char*)malloc(sizeof(char)*(total_inodes+1));

    //initialize blocks_isfree list
    int i;
    int start_data_blocks = 1+(total_inodes+INODENUM)/INODENUM;
    for (i=0; i<total_blocks; i++) {
        if (i<start_data_blocks) blocks_isfree[i] = 'n';
        else blocks_isfree[i] = 'y';
    }
    //initalize inodes_isfree list
    inodes_isfree[0] = 'n';
    for (i=1; i<=total_inodes; i++) {
        inodes_isfree[i] = 'y';
    }
    //scan the disk to build the blocks_isfree list

    check_used_blocks_inodes(ROOTINODE);
}

//free the block and inode list
void terminate(void)
{
    free(blocks_isfree);
    free(inodes_isfree);
}

//find an inode along the path
int path_to_inum(char *pathname, int len_path, int proc_inum, int symlink_cnt)
{
    if (pathname==NULL) return 0;
    int cur_inode = proc_inum;
    if (len_path==0) {
        return cur_inode;
    }

    //get a component in path
    char node_name[DIRNAMELEN];
    memset(node_name,'\0',DIRNAMELEN);
    if (pathname[0]=='/') {
        while (len_path>0 && *pathname=='/') {
            pathname++;
            len_path--;
        }

        cur_inode = ROOTINODE;
        return path_to_inum(pathname,len_path,cur_inode,symlink_cnt);
    }
    int i = 0;
    while (len_path>0 && (*pathname!='/')) {
        node_name[i] = *pathname;
        i++;
        pathname++;
        len_path--;
    }
    while (len_path>0 && *pathname=='/') {
        pathname++;
        len_path--;
    }

    //looking up the inum in cur_inode for the acquired component
    inode_cache *n = read_inode(cur_inode);

    if (n->data.type!=INODE_DIRECTORY) {
        perror("illegal pathname!");
        return -1;
    }

    int sub_inum = check_dir(cur_inode, node_name);
 

    if (sub_inum<=0) {
        //perror("illegal pathname, non-exist directory");
        return -1;
    }
    n = read_inode(sub_inum);
    if (n->data.type==INODE_SYMLINK) {
        if (symlink_cnt>=MAXSYMLINKS) {
            perror("symlink traverse more than MAXSYMLINKS!");
            return -1;
        }
        char *new_pathname = (char*)malloc(sizeof(char)*(n->data.size)+len_path+1);
        block_cache *b = read_block(n->data.direct[0]);
        memcpy(new_pathname,b->data,n->data.size);
        char *t1 = new_pathname+n->data.size;
        *t1 = '/';
        t1++;
        memcpy(t1,pathname,len_path);
        return path_to_inum(new_pathname,n->data.size+1+len_path,cur_inode,symlink_cnt+1);
    }
    else return path_to_inum(pathname,len_path,sub_inum,symlink_cnt);

}

//handling open request
void open_handler(Msg *msg, int sender_pid)
{
    //msg->ptr1 is pathname, msg->num1 is length of pathname, msg->num2 is proc_inode
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1+1);
    int open_inum = path_to_inum(pathname,msg->num1,msg->num2,0);
    if (open_inum<=0) {
        msg->type = ERROR;
    }
    else {
        msg->num1 = open_inum;
        inode_cache *n = read_inode(open_inum);
        msg->num2 = n->data.reuse;
    }
}

//check is 2 strings equal
int isEqual(char* non_null_str, char* null_str)
{
    char *str1=non_null_str;
    while(str1-non_null_str<=DIRNAMELEN&&*str1!='\0'){
        str1++;
    }
    str1--;
    char *str2=null_str+strlen(null_str)-1;
    if(str1-non_null_str!=str2-null_str){
        return 0;
    }
    while(str1>=non_null_str&&str2>=null_str){
        if((*str1)!=(*str2)){
            return 0;
        }
        str1--;
        str2--;
    }
    return 1;
}

//check is file exists in dir
int check_dir(int direct_inum, char* filename)
{
    inode_cache *dir = read_inode(direct_inum);
    if (dir->data.type!=INODE_DIRECTORY) {
        //perror("check_dir: not a dir\n");
        return -1;
    }
    int i,block_num;

    //check direct blocks
    for (i=0; i<NUM_DIRECT; i++) {
        if (dir->data.direct[i]<=0) break;
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        struct dir_entry *d = (struct dir_entry*)(b->data);
        int j;
        for (j=0; j<DIRS_PER_BLOCK; j++) {
            if (d[j].inum<=0) continue;
            int re_equ=isEqual(d[j].name,filename);
            if (re_equ) return d[j].inum;
        }
    }

    //check indirect bloc
    if (dir->data.indirect!=0) {
        block_num = dir->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        block_cache *tmp;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                tmp = read_block(block_num);
                struct dir_entry *d = (struct dir_entry*)(tmp->data);
                int k;
                for (k=0; k<DIRS_PER_BLOCK; k++) {
                    if (d[j].inum<=0) continue;
                    if (isEqual(d[j].name,filename)) return d[j].inum;
                }
            }
            else break;
        }
    }

    //perror("check_dir: return 0\n");
    return 0;
}

//search for dir_entry from parent dir
struct dir_entry *search_dir_entry(int direct_inum, char* filename)
{
    inode_cache *dir = read_inode(direct_inum);
    
    if (dir->data.type!=INODE_DIRECTORY) return NULL;
    int i,block_num;

    //check direct blocks
    for (i=0; i<NUM_DIRECT; i++) {
        if (dir->data.direct[i]<=0) break;
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        struct dir_entry *d = (struct dir_entry*)(b->data);
        int j;
        for (j=0; j<DIRS_PER_BLOCK; j++) {
            if (d[j].inum<=0) continue;
            if (isEqual(d[j].name,filename)){
                dir->dirty=1;
                b->dirty=1;
                return &d[j];
            }
        }
    }

    //check indirect bloc
    if (dir->data.indirect!=0) {
        block_num = dir->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        block_cache *tmp;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                tmp = read_block(block_num);
                struct dir_entry *d = (struct dir_entry*)(tmp->data);
                int k;
                for (k=0; k<DIRS_PER_BLOCK; k++) {
                    if (d[j].inum<=0) continue;
                    if (isEqual(d[j].name,filename)){
			dir->dirty=1;
                        tmp->dirty=1;
                        return &d[j];
                    }
                }
            }
            else break;
        }
    }

    return NULL;
}

//free an inode
void free_inode(int inum)
{
    inode_cache *n = read_inode(inum);
    n->data.size = 0;
    n->dirty = 1;
    int i,block_num;
    
    //free direct block
    for (i=0; i<NUM_DIRECT; i++) {
        if (n->data.direct[i]<=0) break;
        block_num = n->data.direct[i];
        block_cache *b = read_block(block_num);
        memset(b->data,'\0',sizeof(char)*BLOCKSIZE);
        b->dirty = 1;
        blocks_isfree[block_num] = 'y';
    }

    //free indirect block and blocks pointed by content on indirect block
    if (n->data.indirect!=0) {
        block_num = n->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        block_cache *tmp;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                tmp = read_block(block_num);
                memset(tmp->data,'\0',sizeof(char)*BLOCKSIZE);
                tmp->dirty = 1;
                blocks_isfree[block_num] = 'y';
            }
            else break;
        }
        memset(b->data,'\0',sizeof(char)*BLOCKSIZE);
        b->dirty = 1;
        blocks_isfree[n->data.indirect] = 'y';
    }
}

//allocate an inode
int alloc_inode(int type, int pare_inum)
{
    int free_inum,i;
    for (free_inum=1; free_inum<=total_inodes; free_inum++) {
        if (inodes_isfree[free_inum]=='y') break;
    }
    if (free_inum>total_inodes) {
        perror("all inods are taken!");
        return 0;
    }
    inode_cache *n = read_inode(free_inum);
    if (type==INODE_REGULAR) {
        n->data.type = INODE_REGULAR;
        n->dirty = 1;
        n->data.nlink = 1;
        n->data.reuse++;
        n->data.size = 0;
        for (i=0; i<NUM_DIRECT; i++)
            n->data.direct[i] = 0;
        n->data.indirect = 0;
    }
    else if (type==INODE_DIRECTORY) {
        n->data.type = INODE_DIRECTORY;
        n->dirty = 1;
        n->data.nlink = 1;
        n->data.reuse++;
        n->data.size = 2*sizeof(struct dir_entry);
        for (i=1; i<NUM_DIRECT; i++)
            n->data.direct[i] = 0;
        n->data.indirect = 0;
        n->data.direct[0] = alloc_block();
        block_cache *b = read_block(n->data.direct[0]);
        struct dir_entry init[2];
        init[0].inum = free_inum;
        init[0].name[0] = '.';
        init[1].inum = pare_inum;
        init[1].name[0] = '.';
        init[1].name[1] = '.';
        memcpy(b->data,init,2*sizeof(struct dir_entry));
        b->dirty = 1;
    }
    else if (type==INODE_SYMLINK) {

        n->data.type = INODE_SYMLINK;
        n->dirty = 1;
        n->data.nlink = 1;
        n->data.reuse++;
        n->data.size = 0;
        for (i=0; i<NUM_DIRECT; i++)
            n->data.direct[i] = 0;
        n->data.indirect = 0;
        
    }
    inodes_isfree[free_inum] = 'n';
    return free_inum;
}

//get an empty dir_entry under a dir
struct dir_entry *empty_dir(int direct_inum)
{
    inode_cache *dir = read_inode(direct_inum);
    if (dir->data.type!=INODE_DIRECTORY) return NULL;
    int i,block_num;

    //check direct blocks
    for (i=0; i<NUM_DIRECT; i++) {
        if (dir->data.direct[i]<=0) break;
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        struct dir_entry *d = (struct dir_entry*)(b->data);
        int j;
        for (j=0; j<DIRS_PER_BLOCK; j++) {
            if (d[j].inum==0) {
                b->dirty = 1;
                return &d[j];
            }
        }
    }
    if (i<NUM_DIRECT) {
        dir->data.direct[i] = alloc_block();
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        b->dirty = 1;
        return (struct dir_entry*)(b->data);
    }
    
    //check indirect bloc
    if (dir->data.indirect==0) {
        dir->data.indirect = alloc_block();
    }
    block_num = dir->data.indirect;
    block_cache *b = read_block(block_num);
    int j;
    block_cache *tmp;
    for (j=0; j<BLOCKSIZE; j+=4) {
        block_num = *(int*)(b->data+j);
        if (block_num!=0) {
            tmp = read_block(block_num);
            struct dir_entry *d = (struct dir_entry*)(tmp->data);
            int k;
            for (k=0; k<DIRS_PER_BLOCK; k++) {
                if (d[j].inum==0) {
                    tmp->dirty = 1;
                    return d;
                }
            }
        }
        else break;
    }
    if (j==BLOCKSIZE) return NULL;
    block_num = alloc_block();
    *(int*)(b->data+j) = block_num;
    b->dirty = 1;
    tmp = read_block(block_num);
    tmp->dirty = 1;
    return (struct dir_entry*)(tmp->data);
}

//handling create request
void create_handler(Msg *msg, int sender_pid)
{
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1+1);
    char* filename = pathname+msg->num1;
    int direct_len = msg->num1;
    while ((*filename)!='/' && filename!=pathname) {
        filename--;
        direct_len--;
    }
    if ((*filename)=='/') {
        direct_len++;
        filename++;
    }
    if (strlen(filename)==0) {
        perror("invalid pathname when creating file==0!");
        msg->type = ERROR;
        return;
    }
    int direct_inum = path_to_inum(pathname,direct_len,msg->num2,0);
    
    if (direct_inum<=0) {
        perror("invalid pathname when creating file direct_num<0!");
        msg->type = ERROR;
        return;
    }
    int new_inum = check_dir(direct_inum,filename);
    if (new_inum<0) {
        perror("invalid pathname when creating file new_inum<0!");
        msg->type = ERROR;
        return;
    }

    //exist same file name in the directory
    if (new_inum>0) {
        inode_cache *n = read_inode(new_inum);
        free_inode(n->inode_num);
        msg->num1 = new_inum;
    }
    else if (new_inum==0) {
        
        new_inum = alloc_inode(INODE_REGULAR,0);
        struct dir_entry *d = empty_dir(direct_inum);
        if (d==NULL) {
            perror("no empty space for new directory");
            msg->type = ERROR;
            return;
        }
        
        d->inum = new_inum;
        memcpy(d->name,filename,strlen(filename));
        inode_cache *n = read_inode(direct_inum);
        n->data.size += sizeof(struct dir_entry);
        n->data.nlink++;
        n->dirty = 1;
        msg->num1 = new_inum;
        n = read_inode(new_inum);
        msg->num2 = n->data.reuse;
    }
    

}

//handling read request
void read_handler(Msg *msg, int sender_pid)
{
    int inum = msg->num1;
    int to_read = msg->num2;
    int pos = msg->num3;
    inode_cache *n = read_inode(inum);
    int remaining_cnt = n->data.size-pos;
    int read_cnt = to_read<remaining_cnt?to_read:remaining_cnt;
    if (read_cnt<=0) {
        msg->num1 = 0;
        return;
    }
    char *local_buf = (char*)malloc(sizeof(char)*read_cnt);
    int cur = 0,i,block_num,j;
    int end_byte = pos+read_cnt;
    for (i=0; i<NUM_DIRECT; i++) {
        block_num = n->data.direct[i];
        if (block_num<=0) break;
        if (cur<=pos && cur+BLOCKSIZE>pos) break;
        cur += BLOCKSIZE;
    }
    if (i<NUM_DIRECT) {
        char *tmp = local_buf;
        while (pos<end_byte && i<NUM_DIRECT) {
            block_num = n->data.direct[i];
            if (block_num>0) {
                int read_in_block;
                if (end_byte<cur+BLOCKSIZE) read_in_block = end_byte-pos;
                else read_in_block = cur+BLOCKSIZE-pos;
                block_cache *b = read_block(block_num);
                memcpy(tmp,b->data+pos-cur,read_in_block);

                pos += read_in_block;
                tmp += read_in_block;
                cur += BLOCKSIZE;
            }
            i++;
        }
        if (i>=NUM_DIRECT) {
            if (n->data.indirect!=0) {
                block_num = n->data.indirect;
                block_cache *b = read_block(block_num);
                j = 0;
                while (pos<end_byte && j<BLOCKSIZE) {
                    block_num = *(int*)(b->data+j);
                    if (block_num>0) {
                        int read_in_block;
                        if (end_byte<cur+BLOCKSIZE) read_in_block = end_byte-pos;
                        else read_in_block = cur+BLOCKSIZE-pos;
                        block_cache *bc = read_block(block_num);
                        memcpy(tmp,bc->data+pos-cur,read_in_block);
                        pos += read_in_block;
                        tmp += read_in_block;
                        cur += BLOCKSIZE;
                    }
                    j+=4;
                }
            }
        }
    }
    else {
        //check indirect bloc
        if (n->data.indirect!=0) {
            block_num = n->data.indirect;
            block_cache *b = read_block(block_num);
            for (j=0; j<BLOCKSIZE; j+=4) {
                block_num = *(int*)(b->data+j);
                if (block_num!=0) {
                    if (cur<=pos && cur+BLOCKSIZE>pos) break;
                    cur += BLOCKSIZE;
                }
            }
            char *tmp = local_buf;
            while (pos<end_byte && j<BLOCKSIZE) {
                block_num = *(int*)(b->data+j);
                if (block_num>0) {
                    int read_in_block;
                    if (end_byte<cur+BLOCKSIZE) read_in_block = end_byte-pos;
                    else read_in_block = cur+BLOCKSIZE-pos;
                    block_cache *bc = read_block(block_num);
                    memcpy(tmp,bc->data+pos-cur,read_in_block);
                    pos += read_in_block;
                    tmp += read_in_block;
                    cur += BLOCKSIZE;
                }
                j+=4;
            }

        }
    }
    if (CopyTo(sender_pid,msg->ptr1,local_buf,read_cnt)){
        msg->type = ERROR;
        free(local_buf);
        return;
    }
    msg->num1 = read_cnt;//how many bytes have been read
    free(local_buf);
}

//aclocate a block
int alloc_block()
{
    int i;
    for (i=1; i<total_blocks; i++) {
        if (blocks_isfree[i]=='y') break;
    }
    block_cache *b = read_block(i);
    memset(b->data,'\0',BLOCKSIZE);
    b->dirty = 1;
    blocks_isfree[i] = 'n';
    return i;
}

//handling write request
void write_handler(Msg *msg, int sender_pid)
{
    int inum = msg->num1;
    int to_write = msg->num2;
    int pos = msg->num3;
    
    char *local_buf = (char*)malloc(sizeof(char)*to_write);
    CopyFrom(sender_pid,local_buf,msg->ptr1,to_write);
    inode_cache *n = read_inode(inum);
    if (n->data.type==INODE_DIRECTORY) {
        msg->type = ERROR;
        perror("try to write to directory!");
        return;
    }
    int new_block_cnt = 0;
    if (pos+to_write>n->data.size) {
        new_block_cnt = (pos+to_write-n->data.size+BLOCKSIZE-1)/BLOCKSIZE;
    }
    int i,block_num;
    for (i=0; i<NUM_DIRECT; i++) {
        if (new_block_cnt<=0) break;
        if (n->data.direct[i]<=0) {
            n->data.direct[i] = alloc_block();
            n->dirty=1;
            new_block_cnt--;
        }
    }
    if (new_block_cnt>0) {
        if (n->data.indirect==0){
            n->data.indirect = alloc_block();
            n->dirty=1; 
        }
        block_cache *b = read_block(n->data.indirect);
        int j;
        for (j=0; j<BLOCKSIZE; j+=4) {
            if (new_block_cnt<=0) break;
            block_num = *(int*)(b->data+j);
            if (block_num<=0) {
                *(int*)(b->data+j) = alloc_block();
                b->dirty=1;
                new_block_cnt--;
            }
        }
    }

    int cur = 0,j;
    for (i=0; i<NUM_DIRECT; i++) {
        block_num = n->data.direct[i];
        if (block_num<=0) continue;
        if (cur<=pos && cur+BLOCKSIZE>pos) break;
        cur += BLOCKSIZE;
    }
    if (i<NUM_DIRECT) {
        
        char *tmp = local_buf;
        while (to_write>0 && i<NUM_DIRECT) {
            block_num = n->data.direct[i];
            if (block_num>0) {
                int write_in_block;
                if (to_write<cur+BLOCKSIZE-pos) write_in_block = to_write;
                else write_in_block = cur+BLOCKSIZE-pos;
                block_cache *b = read_block(block_num);
                memcpy(b->data+pos-cur,tmp,write_in_block);
                
                b->dirty = 1;
                pos += write_in_block;
                tmp += write_in_block;
                to_write -= write_in_block;
                cur += BLOCKSIZE;
            }
            i++;
        }
        if (i>=NUM_DIRECT) {
            if (n->data.indirect!=0) {
                block_num = n->data.indirect;
                block_cache *b = read_block(block_num);
                j = 0;
                while (to_write>0 && j<BLOCKSIZE) {
                    block_num = *(int*)(b->data+j);
                    if (block_num>0) {
                        int write_in_block;
                        if (to_write<cur+BLOCKSIZE-pos) write_in_block = to_write;
                        else write_in_block = cur+BLOCKSIZE-pos;
                        block_cache *b = read_block(block_num);
                        memcpy(b->data+pos-cur,tmp,write_in_block);
                        b->dirty = 1;
                        pos += write_in_block;
                        tmp += write_in_block;
                        to_write -= write_in_block;
                        cur += BLOCKSIZE;
                    }
                    j+=4;
                }
            }
        }
    }
    else {
        //check indirect bloc
        if (n->data.indirect!=0) {
            block_num = n->data.indirect;
            block_cache *b = read_block(block_num);
            for (j=0; j<BLOCKSIZE; j+=4) {
                block_num = *(int*)(b->data+j);
                if (block_num!=0) {
                    if (cur<=pos && cur+BLOCKSIZE>pos) break;
                    cur += BLOCKSIZE;
                }
            }
            char *tmp = local_buf;
            while (to_write>0 && j<BLOCKSIZE) {
                block_num = *(int*)(b->data+j);
                if (block_num>0) {
                    int write_in_block;
                    if (to_write<cur+BLOCKSIZE-pos) write_in_block = to_write;
                    else write_in_block = cur+BLOCKSIZE-pos;
                    block_cache *bc = read_block(block_num);
                    memcpy(bc->data+pos-cur,tmp,write_in_block);
                    bc->dirty = 1;
                    pos += write_in_block;
                    tmp += write_in_block;
                    to_write -= write_in_block;
                    cur += BLOCKSIZE;
                }
                j+=4;
            }

        }
    }
    if(n->data.size<pos)
        n->data.size=pos; 
    msg->num1 = msg->num2 - to_write;

}

//handling mkdir request
void mkdir_handler(Msg *msg, int sender_pid)
{
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1+1);
    char* dir_name = pathname+msg->num1;
    int direct_len = msg->num1;
    while ((*dir_name)!='/' && dir_name!=pathname) {
        dir_name--;
        direct_len--;
    }
    if ((*dir_name)=='/') {
        direct_len++;
        dir_name++;
    }
    if (strlen(dir_name)==0) {
        perror("invalid pathname when creating file!");
        msg->type = ERROR;
        return;
    }
    int direct_inum = path_to_inum(pathname,direct_len,msg->num2,0);
    if (direct_inum<=0) {
        perror("invalid pathname when creating file!");
        msg->type = ERROR;
        return;
    }
    int new_inum = check_dir(direct_inum,dir_name);
    if (new_inum<0) {
        perror("invalid pathname when creating file!");
        msg->type = ERROR;
        return;
    }

    //exist same file name in the directory
    else if (new_inum>0) {
        perror("exist a directory with same name");
        msg->type = ERROR;
        return;
    }
    else if (new_inum==0) {
        new_inum = alloc_inode(INODE_DIRECTORY,direct_inum);
        struct dir_entry *d = empty_dir(direct_inum);
        if (d==NULL) {
            perror("no empty space for new directory");
            msg->type = ERROR;
            return;
        }
        d->inum = new_inum;
        memcpy(d->name,dir_name,strlen(dir_name));
        inode_cache *n = read_inode(direct_inum);
        n->data.nlink++;
        n->data.size+=sizeof(struct dir_entry);
        n->dirty = 1;
        msg->num1 = 0;
    }
}

//handling rmdir request
void rmdir_handler(Msg *msg, int sender_pid)
{
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1);
    int rm_inum = path_to_inum(pathname,msg->num1,msg->num2,0);
    if (rm_inum<=0) {
        msg->type = ERROR;
        return;
    }
    inode_cache *n = read_inode(rm_inum);
    if (n->data.size>2*sizeof(struct dir_entry)) {
        perror("try to remove a non-empty directory!");
        msg->type = ERROR;
        return;
    }
    block_cache *b = read_block(n->data.direct[0]);
    struct dir_entry *d = (struct dir_entry*)(b->data);
    int pare_inum = d[1].inum;
   
    free_inode(rm_inum);

    //remove the dir_entry of rm_inum in parent directory
    inode_cache *dir = read_inode(pare_inum);
    if (dir->data.type!=INODE_DIRECTORY) {
        perror("parent not a directory when remove dir");
        return;
    }

    dir->data.size-=sizeof(struct dir_entry);
    int i,block_num;

    //check direct blocks
    for (i=0; i<NUM_DIRECT; i++) {
        if (dir->data.direct[i]<=0) continue;
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        struct dir_entry *d = (struct dir_entry*)(b->data);
        int j;
        for (j=0; j<DIRS_PER_BLOCK; j++) {
            if (d[j].inum<=0) continue;
            if (d[j].inum==rm_inum) {
                d[j].inum = 0;
                memset(d[j].name,'\0',DIRNAMELEN);
                b->dirty = 1;
                return;
            }
        }
    }

    //check indirect bloc
    if (dir->data.indirect!=0) {
        block_num = dir->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        block_cache *tmp;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                tmp = read_block(block_num);
                struct dir_entry *d = (struct dir_entry*)(tmp->data);
                int k;
                for (k=0; k<DIRS_PER_BLOCK; k++) {
                    if (d[j].inum<=0) continue;
                    if (d[j].inum==rm_inum) {
                        d[j].inum = 0;
                        memset(d[j].name,'\0',DIRNAMELEN);
                        tmp->dirty = 1;
                        return;
                    }
                }
            }
        }
    }

}

//handling chdir request
void chdir_handler(Msg *msg, int sender_pid)
{
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1+1);
    int target_inum = path_to_inum(pathname,msg->num1,msg->num2,0);
    if (target_inum<=0) {
        perror("illegal destination directory!");
        msg->type = ERROR;
        return;
    }
    inode_cache *n = read_inode(target_inum);
    if (n->data.type!=INODE_DIRECTORY) {
        perror("trying to change current directory to a non-directory place");
        msg->type = ERROR;
        return;
    }
    msg->num1 = target_inum;
}


//handling link request
void link_handler(Msg *msg, int sender_pid)
{

    char oldname[MAXPATHNAMELEN], newname[MAXPATHNAMELEN];

    if(path_to_inum(newname,msg->num3,msg->num1,0)>0){
        /*newname exists*/
        msg->type=-1;
        return;
    }

    memset(oldname,'\0',MAXPATHNAMELEN);
    memset(newname,'\0',MAXPATHNAMELEN);
    CopyFrom(sender_pid,oldname,msg->ptr1,msg->num2+1);
    CopyFrom(sender_pid,newname,msg->ptr2,msg->num3+1);
    int dir_len=msg->num3;
    char *dir_newname=newname+msg->num3;
    while((*dir_newname)!='/'&&dir_newname!=newname){
        dir_len--;
        dir_newname--;
    }

    if((*dir_newname)=='/'){
        dir_len++;
        dir_newname++;
    }
    int parent_inum=path_to_inum(newname,dir_len,msg->num1,0); 
    
    int dir_len_old=msg->num2;
    char *dir_oldname=oldname+msg->num2;
    while((*dir_oldname)!='/'&&dir_oldname!=oldname){
        dir_len_old--;
        dir_oldname--;
    }

    if((*dir_oldname)=='/'){
        dir_len_old++;
        dir_oldname++;
    }

    int parent_old_inum=path_to_inum(oldname,dir_len_old,msg->num1,0);
    int old_inum=check_dir(parent_old_inum,dir_oldname);

    if(old_inum<=0){
        msg->type=-1;
        return;
    }

    inode_cache *old_inode=read_inode(old_inum);
    if(old_inode->data.type==INODE_DIRECTORY){
        msg->type=-1;
        return;
    }

    struct dir_entry *newlink=search_dir_entry(parent_inum,dir_newname);
    if(newlink!=NULL){
        msg->type=-1;
        return;
    }
    if((newlink=empty_dir(parent_inum))==NULL){
        msg->type=-1;
        return;
    }
 
    inode_cache *parent_inode=read_inode(parent_inum);
    parent_inode->data.size+=sizeof(struct dir_entry);
    parent_inode->dirty=1;
    newlink->inum=old_inum;
    memcpy(newlink->name,dir_newname,msg->num3-dir_len);
    old_inode->data.nlink++;
    old_inode->dirty=1;

}

//handling unlink request
void unlink_handler(Msg *msg, int sender_pid)
{

    char pathname[MAXPATHNAMELEN];
    memset(pathname,'\0',MAXPATHNAMELEN);
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num2+1);

    int dir_len=msg->num2;
    char *dir_pathname=pathname+msg->num2;
    while((*dir_pathname)!='/'&&dir_pathname!=pathname){
        dir_len--;
        dir_pathname--;
    }

    if((*dir_pathname)=='/'){
        dir_len++;
        dir_pathname++;
    }
    int parent_inum=path_to_inum(pathname,dir_len,msg->num1,0); 

    int path_inum=check_dir(parent_inum,dir_pathname);

    if(path_inum<=0||parent_inum<=0){
        msg->type=-1;
        return;
    }

    inode_cache *path_inode=read_inode(path_inum);
    inode_cache *parent_inode=read_inode(parent_inum);
    if(path_inode->data.type==INODE_DIRECTORY){
        msg->type=-1;
        return;
    }

    struct dir_entry *path_dir_entry=search_dir_entry(parent_inum,dir_pathname);
    


    if(path_inode->data.nlink>1){
        path_dir_entry->inum=0;
        path_inode->data.nlink--;
        memset(path_dir_entry->name,'\0',DIRNAMELEN);
        parent_inode->data.size-=sizeof(struct dir_entry);
        parent_inode->dirty=1;
        
    }
    else if(path_inode->data.nlink==1){
        path_dir_entry->inum=0;
        path_inode->data.nlink--;
        memset(path_dir_entry->name,'\0',DIRNAMELEN);
        free_inode(path_inum);//XXX
        path_inode->dirty=1;
        parent_inode->data.size-=sizeof(struct dir_entry);
        parent_inode->dirty=1;
    }
    else{
        msg->type=-1;
        return;
    }

}

//handling symlink request
void symlink_handler(Msg *msg, int sender_pid)
{

    char oldname[MAXPATHNAMELEN], newname[MAXPATHNAMELEN];
    memset(oldname,'\0',MAXPATHNAMELEN);
    memset(newname,'\0',MAXPATHNAMELEN);
    CopyFrom(sender_pid,oldname,msg->ptr1,msg->num2+1);
    CopyFrom(sender_pid,newname,msg->ptr2,msg->num3+1);

    int dir_len=msg->num3;
    char *dir_newname=newname+msg->num3;
    while((*dir_newname)!='/'&&dir_newname!=newname){
        dir_len--;
        dir_newname--;
    }

    if((*dir_newname)=='/'){
        dir_len++;
        dir_newname++;
    }
    int parent_inum=path_to_inum(newname,dir_len,msg->num1,0); 
    int sym_inum=alloc_inode(INODE_SYMLINK,parent_inum);
    if(sym_inum<=0){
        msg->type=-1;
        return;
    }

    inode_cache *parent_inode=read_inode(parent_inum);
    inode_cache *sym_inode=read_inode(sym_inum);
    int sym_bnum=alloc_block();
    block_cache *sym_block=read_block(sym_bnum);

    struct dir_entry *sym_dir_entry=search_dir_entry(parent_inum,dir_newname);
    if(sym_dir_entry!=NULL){
        msg->type=-1;
        return;
    }
    if((sym_dir_entry=empty_dir(msg->num1))==NULL){
        msg->type=-1;
        return;
    }

    sym_dir_entry->inum=sym_inum;
    memcpy(sym_dir_entry->name,dir_newname,msg->num3-dir_len);


    sym_inode->data.size=msg->num2;
    sym_inode->data.nlink=1;
    sym_inode->data.direct[0]=sym_bnum;

    sym_block->dirty=1;
    memcpy(sym_block->data,oldname,msg->num2);
    parent_inode->data.size+=sizeof(struct dir_entry);
    parent_inode->dirty=1;

}

//handling readlink request
void readlink_handler(Msg *msg, int sender_pid)
{

    char symname[MAXPATHNAMELEN];
    memset(symname,'\0',MAXPATHNAMELEN);
    CopyFrom(sender_pid,symname,msg->ptr1,msg->num2+1);
    int return_len=msg->num3;

    char* filename = symname+msg->num2;
    int direct_len = msg->num2;
    while ((*filename)!='/' && filename!=symname) {
        filename--;
        direct_len--;
    }
    if ((*filename)=='/') {
        direct_len++;
        filename++;
    }

    int parent_inum=path_to_inum(symname,direct_len,msg->num1,0);
    int sym_inum=check_dir(parent_inum,filename);
    if(sym_inum<=0){
        msg->type=-1;
        return;
    }
    inode_cache *sym_inode=read_inode(sym_inum);
    block_cache *sym_block=read_block(sym_inode->data.direct[0]);
    if(sym_inode->data.size<return_len)
        return_len=sym_inode->data.size;

    CopyTo(sender_pid,msg->ptr2,sym_block->data,return_len);

    msg->num3=return_len;

}
//handling stat request
void stat_handler(Msg *msg, int sender_pid)
{
    char pathname[MAXPATHNAMELEN];
    CopyFrom(sender_pid,pathname,msg->ptr1,msg->num1+1);

    char* filename = pathname+msg->num2;
    int direct_len = msg->num2;
    while ((*filename)!='/' && filename!=pathname) {
        filename--;
        direct_len--;
    }
    if ((*filename)=='/') {
        direct_len++;
        filename++;
    }

    int parent_inum = path_to_inum(pathname,direct_len,msg->num2,0);
    int inum=check_dir(parent_inum,filename);
    struct Stat s;
    inode_cache *n = read_inode(inum);
    s.inum = inum;
    s.type = n->data.type;
    s.size = n->data.size;
    s.nlink = n->data.nlink;
    CopyTo(sender_pid,msg->ptr2,&s,sizeof(struct Stat));

}

//handling sync request
void sync_handler(Msg *msg)
{

    if(sync()==-1)
        msg->type = ERROR;
}

//helper func for sync
int sync()
{

    inode_cache *n = first_inode;
    while (n!=NULL) {
        int inode_num = n->inode_num;
        if (n->dirty!=0) {
            n->dirty = 0;
            int block_num = inode_to_block(inode_num);
            block_cache *b = read_block(block_num);
            int idx = inode_num-(block_num-1)*(BLOCKSIZE/INODESIZE);
            memcpy((void*)(b->data+idx*INODESIZE),(void*)(&(n->data)),INODESIZE);
            b->dirty = 1;
        }
        n = n->next;
    }
    block_cache *t = first_block;
    while (t!=NULL) {
        if (t->dirty){
            t->dirty = 0;
            if (WriteSector(t->block_num,(void*)(t->data))!=0) {
                perror("erro writing back dirty block!");
                return -1;
            }
        }
        t = t->next;
    }
    return 0;    

}

//handling shuntdown request
void shutdown_handler(Msg *msg, int sender_pid)
{
    sync_handler(msg);
    if (msg->type == ERROR) {
        perror("unable to sync");
        return;
    }
    if (Reply(msg,sender_pid)==ERROR) fprintf(stderr, "Error replying to pid %d\n",sender_pid);
    terminate();
    Exit(0);
}

int main(int argc, char* argv[])
{
    int pid;
    Msg myMsg;
    int sender_pid;

    initialize();

    if (Register(FILE_SERVER)) {
        perror("Register server process error!");
    }
    if (argc>1) {
        pid = Fork();
        if (pid==0) {
            Exec(argv[1],argv+1);
        }
    }
    while (1) {
        if ((sender_pid=Receive(&myMsg))==ERROR) {
//            perror("error receiving message!");
            continue;
        }
        switch (myMsg.type) {
            case OPEN:
                open_handler(&myMsg,sender_pid);break;
            /*
            case CLOSE:
                close_handler(&myMsg,sender_pid);break;
            */
            case CREATE:
                create_handler(&myMsg,sender_pid);break;
            case READ:
                read_handler(&myMsg,sender_pid);break;
            case WRITE:
                write_handler(&myMsg,sender_pid);break;
            /*
            case SEEK:
                seek_handler(&myMsg,sender_pid);break;
            */
            case LINK:
                link_handler(&myMsg,sender_pid);break;
            case UNLINK:
                unlink_handler(&myMsg,sender_pid);break;
            case SYMLINK:
                symlink_handler(&myMsg,sender_pid);break;
            case READLINK:
                readlink_handler(&myMsg,sender_pid);break;
            case MKDIR:
                mkdir_handler(&myMsg,sender_pid);break;
            case RMDIR:
                rmdir_handler(&myMsg,sender_pid);break;
            case CHDIR:
                chdir_handler(&myMsg,sender_pid);break;
            case STAT:
                stat_handler(&myMsg,sender_pid);break;
            case SYNC:
                sync_handler(&myMsg);break;
            case SHUTDOWN:
                shutdown_handler(&myMsg,sender_pid);break;
            default:
                perror("message type error!");
                break;
        }
        if (Reply(&myMsg,sender_pid)==ERROR) fprintf(stderr, "Error replying to pid %d\n",sender_pid);
    }
    terminate();
    return 0;
}

//debug func print all entries on the dir
void print_dir_entries(int direct_inum) {
    inode_cache *dir = read_inode(direct_inum);
    if (dir->data.type!=INODE_DIRECTORY) {
        perror("print_dir: not a dir\n");
        return;
    }
    int i,block_num;
    
    //check direct blocks
    for (i=0; i<NUM_DIRECT; i++) {
        if (dir->data.direct[i]<=0) break;
        block_num = dir->data.direct[i];
        block_cache *b = read_block(block_num);
        struct dir_entry *d = (struct dir_entry*)(b->data);
        int j;
        for (j=0; j<DIRS_PER_BLOCK; j++) {
            if (d[j].inum<=0) continue;
            printf("direct_index=%d\tblock_num=%d\td[%d].inum=%d\td[%d].name=%s\n",i,block_num,j,d[j].inum,j,d[j].name);
        }
    }
    
    //check indirect bloc
    if (dir->data.indirect!=0) {
        block_num = dir->data.indirect;
        block_cache *b = read_block(block_num);
        int j;
        block_cache *tmp;
        for (j=0; j<BLOCKSIZE; j+=4) {
            block_num = *(int*)(b->data+j);
            if (block_num!=0) {
                tmp = read_block(block_num);
                struct dir_entry *d = (struct dir_entry*)(tmp->data);
                int k;
                for (k=0; k<DIRS_PER_BLOCK; k++) {
                    if (d[j].inum<=0) continue;
                    printf("indirect: block_num=%d\td[%d].inum=%d\td[%d].name=%s\n",i,block_num,d[j].inum,j,d[j].name);
                }
            }
            else break;
        }
    }
}

