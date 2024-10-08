#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
int main() {
	char *filename = "/dev/ir_device";
	int fd = open(filename, O_RDWR,0);
	
	char buf[6];
	pread(fd,buf,sizeof(buf),0);
	printf("%s\n",buf);
	write(fd,"OFF", 3);
	printf("Did the LED turned off?\n");
	sleep(5);
	write(fd,"ON",2);
	printf("Did the LED turned on?\n");
	sleep(5);
	return 0;
}

