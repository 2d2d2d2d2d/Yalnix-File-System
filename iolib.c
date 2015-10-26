//
//  iolib.c
//  yfs
//
//  Created by Wenzhe Jiang & Jun Liu on 4/19/15.
//  Copyright (c) 2015 Wenzhe Jiang & Jun Liu. All rights reserved.
//

#include "yfs.h"

typedef struct file_info {
    int inum;
    int reuse;
    int pos;
    int size;
} file_info;

file_info open_files[MAX_OPEN_FILES];
int cur_inode = ROOTINODE;
int initialized = 0;

void init() {
    if (initialized) return;
    int i;
    for (i=0; i<MAX_OPEN_FILES; i++) {
        open_files[i].inum = 0;
        open_files[i].pos = 0;
        open_files[i].reuse = 0;
        open_files[i].size = 0;
    }
    initialized = 1;
}
int unused_fd() {
    int i;
    for (i=0; i<MAX_OPEN_FILES; i++)
        if (open_files[i].inum==0) return i;
    return -1;
}
int alreayd_open(int inum) {
    int i;
    for (i=0; i<MAX_OPEN_FILES; i++) {
        if (inum!=0 && open_files[i].inum==inum) return i;
    }
    return -1;
}
int Open(char *pathname) {
    init();
    Msg msg;
    msg.type = OPEN;
    msg.ptr1 = pathname;
    msg.num1 = (int)strlen(pathname);
    msg.num2 = cur_inode;
    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending open message");
        return ERROR;
    }
    if (msg.type==OPEN) {
        int fd = alreayd_open(msg.num1);
        if (fd<0) {
            fd = unused_fd();
            if (fd==-1) {
                perror("Error: too many open files!");
                return ERROR;
            }
        }
        open_files[fd].inum = msg.num1;
        open_files[fd].pos = 0;
        open_files[fd].reuse = msg.num2;
        return fd;
    }
    return ERROR;
}

int Close(int fd) {
    init();
    if (fd<0 || fd>=MAX_OPEN_FILES) {
        perror("invalid fd!");
        return ERROR;
    }
    open_files[fd].inum = 0;
    open_files[fd].pos = 0;
    open_files[fd].reuse = 0;
    return 0;
}

int Create(char *pathname) {
    init();
    
    Msg msg;
    msg.type = CREATE;
    msg.ptr1 = pathname;
    msg.num1 = (int)strlen(pathname);
    msg.num2 = cur_inode;
   

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending create message");
        return ERROR;
    }
    if (msg.type==CREATE) {
        int fd = alreayd_open(msg.num1);
        if (fd<0) {
            fd = unused_fd();
            if (fd==-1) {
                perror("Error: too many open files!");
                return ERROR;
            }
        }
        open_files[fd].inum = msg.num1;
        open_files[fd].pos = 0;
        open_files[fd].reuse = msg.num2;
        return fd;
    }
    return ERROR;
}

int Read(int fd, void *buf, int size) {
    init();
    if (fd<0 || fd>=MAX_OPEN_FILES) {
        perror("invalid fd!");
        return ERROR;
    }
    if (open_files[fd].inum==0) {
        perror("the file is not open yet!");
        return ERROR;
    }
    Msg msg;
    msg.type = READ;
    msg.ptr1 = buf;
    msg.num1 = open_files[fd].inum;
    msg.num2 = size;
    msg.num3 = open_files[fd].pos;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending read message");
        return ERROR;
    }

    if (msg.type==READ) {
        open_files[fd].pos += msg.num1;
        return msg.num1;
    }
    return ERROR;
}

int Write(int fd, void *buf, int size) {
    init();
    if (fd<0 || fd>=MAX_OPEN_FILES) {
        perror("invalid fd!");
        return ERROR;
    }
    if (open_files[fd].inum==0) {
        perror("the file is not open yet!");
        return ERROR;
    }

    Msg msg;
    msg.type=WRITE;
    msg.num1=open_files[fd].inum;
    msg.num2=size;
    msg.num3=open_files[fd].pos;
    msg.ptr1=buf;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending read message");
        return ERROR;
    }

    if (msg.type==WRITE) {
        open_files[fd].pos += msg.num1;
        if(open_files[fd].pos+1>open_files[fd].size)
        open_files[fd].size = open_files[fd].pos;
        return msg.num1;
    }
    return ERROR;

}

int Seek(int fd, int offset, int whence) {
    init();
    if (fd<0 || fd>=MAX_OPEN_FILES) {
        perror("invalid fd!");
        return ERROR;
    }
    if (open_files[fd].inum==0) {
        perror("the file is not open yet!");
        return ERROR;
    }

    switch (whence){
        case SEEK_SET:
            if(offset>=0&&offset<=open_files[fd].size){
                open_files[fd].pos=0+offset;
                return open_files[fd].pos;
            }
            break;
        case SEEK_CUR:
            if(offset+open_files[fd].pos>=0&&offset+open_files[fd].pos<=open_files[fd].size){
                open_files[fd].pos+=offset;
                return open_files[fd].pos;
            }
            break;
        case SEEK_END:
            if(open_files[fd].size+offset>=0&&offset<=0){
                open_files[fd].pos=open_files[fd].size+offset;
                return open_files[fd].pos;
            }
            break;

    }
    perror("invalid offset!");
    return ERROR;

}

int Link(char *oldname, char *newname) {
    init();
    Msg msg;
    msg.type=LINK;
    msg.num1=cur_inode;
    msg.num2=strlen(oldname);
    msg.num3=strlen(newname);
    msg.ptr1=oldname;
    msg.ptr2=newname;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending link message");
        return ERROR;
    }

    if (msg.type==LINK) {
        return 0;
    }
    else
        return ERROR;

}

int Unlink(char *pathname) {
    init();
    Msg msg;
    msg.type=UNLINK;
    msg.num1=cur_inode;
    msg.num2=strlen(pathname);
    msg.ptr1=pathname;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending link message");
        return ERROR;
    }

    if (msg.type==UNLINK) {
        return 0;
    }
    else
        return ERROR;

}

int SymLink(char *oldname, char *newname) {
    init();
    if(oldname==NULL||newname==NULL)
        return ERROR;
    Msg msg;
    msg.type=SYMLINK;
    msg.num1=cur_inode;
    msg.num2=strlen(oldname);
    msg.num3=strlen(newname);
    msg.ptr1=oldname;
    msg.ptr2=newname;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending symlink message");
        return ERROR;
    }

    if (msg.type==SYMLINK) {
        return 0;
    }
    else
        return ERROR;

}

int ReadLink(char *pathname, char *buf, int len) {
    init();
    Msg msg;
    msg.type=READLINK;
    msg.num1=cur_inode;
    msg.num2=strlen(pathname);
    msg.num3=len;
    msg.ptr1=pathname;
    msg.ptr2=buf;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending readlink message");
        return ERROR;
    }

    if (msg.type==READLINK) {
        return msg.num3;
    }
    else
        return ERROR;
}

int MkDir(char *pathname) {
    init();
    Msg msg;
    msg.type=MKDIR;
    msg.num1=strlen(pathname);
    msg.ptr1=pathname;
    msg.num2=cur_inode;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending mkdir message");
        return ERROR;
    }

    if (msg.type==MKDIR) {
        return 0;
    }
    else
        return ERROR;

}

int RmDir(char *pathname) {
    init();
    Msg msg;
    msg.type=RMDIR;
    msg.num1=strlen(pathname);
    msg.ptr1=pathname;
    msg.num2=cur_inode;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending rmdir message");
        return ERROR;
    }

    if (msg.type==RMDIR) {
        return 0;
    }
    else
        return ERROR;
}
int ChDir(char *pathname) {
    init();
    Msg msg;
    msg.type=CHDIR;
    msg.num1=strlen(pathname);
    msg.ptr1=pathname;
    msg.num2=cur_inode;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending chdir message");
        return ERROR;
    }

    if (msg.type==CHDIR) {
        cur_inode=msg.num1;
        return 0;
    }
    else
        return ERROR;
}
int Stat(char *pathname, struct Stat *statbuf) {
    init();
    Msg msg;
    msg.type=STAT;
    msg.ptr1=pathname;
    msg.num1=strlen(pathname);
    msg.ptr2=statbuf;
    msg.num2=cur_inode;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending stat message");
        return ERROR;
    }

    if (msg.type==STAT) {
        return 0;
    }
    else
        return ERROR;
}
int Sync(void) {
    init();
    Msg msg;
    msg.type=SYNC;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending sync message");
        return ERROR;
    }

    if (msg.type==SYNC) {
        return 0;
    }
    else
        return ERROR;
}
int Shutdown(void) {
    init();
    Msg msg;
    msg.type=SHUTDOWN;

    if (Send(&msg,-FILE_SERVER)==ERROR) {
        perror("error sending shutdown message");
        return ERROR;
    }

    if (msg.type==SHUTDOWN) {
        printf("file server is shutdown!\n");
        return 0;
    }
    else
        return ERROR;
}
