#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define N 1024

char buf[N] = {0};
char filename[64] = {0};

void updata(int connfd)
{
	int fd;
	int n;
	long int file_len;

	n = read(connfd,&buf[0],1);
	n = read(connfd,filename,buf[0]);		
    n = read(connfd,&file_len,4); 
	fd = open(filename,O_RDWR | O_CREAT | O_TRUNC, 0664);
	
	while((n = read(connfd,buf,N)) > 0)
	{
		write(fd,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n; 
		if(file_len == 0)
			break;
	}
	printf("\n");
	printf("上传成功...\n");
	close(fd);
}

void download(int connfd)
{
	int fd;
	int n;
	struct stat st;
	long int file_len;

	n = read(connfd,&buf[0],1);
	n = read(connfd,filename,buf[0]);
	fd = open(filename,O_RDONLY);
	stat(filename,&st);
	file_len = st.st_size;
	n = write(connfd,&file_len,4);
	
	while((n = read(fd,buf,N)) > 0)
	{
		write(connfd,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n;
		if(file_len == 0)
			break;
	}
	printf("\n");
	printf("下载成功...\n");

	close(fd);
}

void handler(char cmd,int connfd)
{
	switch(cmd)
	{
		case 'U':
			printf("正在从客户端上传...\n");
			updata(connfd);
			break;
		case 'D':
			printf("正在从服务器下载\n");
			download(connfd);
			break;
		default:
			printf("接收的命令有误!\n");
	}
	bzero(buf,N); 
	bzero(filename,64); 
}

int main(int argc, const char *argv[])
{
	int sockfd;
	int connfd;
	struct sockaddr_in  serveraddr;
	struct sockaddr_in  clientaddr;
	socklen_t addrlen;
	char cmd;
	
	if(argc < 3)
	{
		fprintf(stderr,"Usage:%s ipaddr port\n",argv[0]);
		return -1;
	}

	//1.建立连接
	sockfd = socket(AF_INET , SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("fail to socket");
		return -1;
	}
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));
	if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("fail to bind");
		return -1;
	}
	if(listen(sockfd, 5) < 0)
	{
		perror("fail to listen");
		return -1;
	}
	addrlen = sizeof(clientaddr);
	
	while(1)  //----->while(1)可以响应多个客户端
	{
		connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &addrlen);
		if(connfd < 0)
		{
			perror("fail to accept");
			return -1;
		}

		//2.响应客户端的请求
		while(1)  //----->while(1)可以响应一个客户端的多次请求
		{
			//(1).读取命令
			if(read(connfd,&cmd,1) < 1)
			{
				close(connfd);
				break;
			}
			//(2).执行命令
			if(cmd == 'Q')
			{
				close(connfd);
				break;
			}
			else
				handler(cmd,connfd);
		}
		printf("connfd--> %d\n",connfd);
		printf("clientaddr ip--> %s\n",(char *)inet_ntoa(clientaddr.sin_addr));
		printf("clientaddr port--> %d\n",ntohs(clientaddr.sin_port));
		printf("客户端退出!\n");

	}
	//3.关闭连接
	close(sockfd);

	return 0;
}
