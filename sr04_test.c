#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
int main() {
	char *filename = "/dev/sr04";
	int fd = open(filename, O_RDONLY,0);
	
	char buf[5];
	pread(fd,buf,sizeof(buf),0);
	printf("%s\n",buf);
	return 0;
}

