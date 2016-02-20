#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define N 1024

char buf[N] = {0};
char filename[64] = {0};

void updata(int sockfd)
{
	int fd;
	int n;
	struct stat st;
	long int file_len;

	//(1).发送上传命令
	buf[0] = 'U';
	n = write(sockfd,&buf[0],1);
	//(2.1).发送文件名长度
	printf("请输入要上传的文件名:\n");
	scanf("%s",buf);
	strcpy(filename,buf);
	buf[0] = strlen(filename);
	n = write(sockfd,&buf[0],1);
	//(2.2).发送文件名
	n = write(sockfd,filename,buf[0]);
	//(3).发送文件长度
	fd = open(filename,O_RDONLY);
	
	stat(filename,&st);
	file_len = st.st_size;
	n = write(sockfd,&file_len,4);
	//(4).发送文件内容
	while((n = read(fd,buf,N)) > 0)
	{
		write(sockfd,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n;
		if(file_len == 0)
			break;
	}
	printf("\n");
	printf("上传成功...\n");
err:
	close(fd);
}
void download(int sockfd)
{
	int fd;
	int n;
	long int file_len;

	//(1).发送下载命令
	buf[0] = 'D';
	n = write(sockfd,&buf[0],1);
	//(2.1).发送文件名长度
	printf("请输入要下载的文件名:\n");
	scanf("%s",buf);
	strcpy(filename,buf);
	buf[0] = strlen(filename);
	n = write(sockfd,&buf[0],1);
	//(2.2).发送文件名
	n = write(sockfd,filename,buf[0]);
	//(2.3).创建文件
	fd = open(filename,O_RDWR | O_CREAT | O_TRUNC, 0664);
	//(3).接收文件长度
    n = read(sockfd,&file_len,4);
	//(4).接收文件内容
	while((n = read(sockfd,buf,N)) > 0)
	{
		write(fd,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n;
		if(file_len == 0)
			break;
	}
	printf("\n");
	printf("下载成功...\n");
err:
	close(fd);
}
void menu(int sockfd)
{
	int num;
 
	while(1)
	{
		printf("1:上传\n");
		printf("2:下载\n");
		printf("3:退出\n");
		scanf("%d",&num);
		switch(num)
		{
		case 1:
			printf("正在上传...\n");
			updata(sockfd);
			break;
		case 2:
			printf("正在下载...\n");
			download(sockfd);
			break;
		case 3:
			system("clear");
			printf("退出...\n");
			goto end;
		default :
			printf("请按照提示输入正确的选项!\n");
		}
		bzero(buf,N); 
		bzero(filename,64); 
	}
end:
	buf[0] = 'Q';
	write(sockfd,&buf[0],1);	
}

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr;

	if(argc < 3)
	{
		fprintf(stderr,"Usage:%s ipaddr port\n",argv[0]);
		return -1;
	}

	//1.建立连接
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("fail to socket");
		return -1;
	}

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

	if(connect(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("fail to connect");
		return -1;
	}
#if 1
	//2.实现上传和下载的菜单
	menu(sockfd);
#else
	printf("**************************\n");
#endif	
	//3.关闭连接
	close(sockfd);

	return 0;
}

