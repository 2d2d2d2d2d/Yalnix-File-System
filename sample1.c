#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>

int
main()
{
	int fd;

	fd = Create("a");
	Write(fd, "aaaaaaaaaaaaaaaa", 16);
	Close(fd);
        fd = Open("a");

        Seek(fd,2,SEEK_SET);
	Write(fd, "b", 1);
	Close(fd);
        
        

        fd = Open("a");
        char read_a[10];
        int cnt_a=Read(fd,read_a,4);
        printf("read_a=%c %c %c %c\t cnt_a=%d\n",read_a[0],read_a[1],read_a[2],read_a[3],cnt_a); 

        int fd1;
	fd1 = Create("b");
        printf("after create b fd = %d\n",fd1);
	Write(fd, "bbbbbbbbbbbbbbbb", 16);
	Close(fd1);

	fd = Create("c");
        printf("after create c fd = %d\n",fd);
	Write(fd, "cccccccccccccccc", 16);
	Close(fd);
        printf("after close c\n");

	MkDir("dir");

        ChDir("dir");

	fd = Create("a");
	Close(fd);

        Unlink("a");
        ChDir("/");
        RmDir("dir");
/*

        printf("before create  /dir/x\n");
	fd = Create("/dir/x");
        printf("after create  /dir/x fd =%d\n",fd);
        printf("before write  /dir/x\n");
	Write(fd, "xxxxxxxxxxxxxxxx", 16);
        printf("after write  /dir/x\n");
        printf("before close  /dir/x\n");
	Close(fd);
        printf("after close  /dir/x\n");

	fd = Create("/dir/y");
	Write(fd, "yyyyyyyyyyyyyyyy", 16);
	Close(fd);

	fd = Create("/dir/z");
	Write(fd, "zzzzzzzzzzzzzzzz", 16);
	Close(fd);
*/
	Shutdown();
}
