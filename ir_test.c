#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
int main() {
	char *filename = "/dev/ir_device";
	int fd = open(filename, O_RDONLY,0);
	
	char buf[6];
	pread(fd,buf,sizeof(buf),0);
	printf("%s\n",buf);
	return 0;
}

