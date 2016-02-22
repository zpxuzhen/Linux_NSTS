#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <errno.h>

#define N 1024

#define logs(n,buf)	if (n < 0) \
		printf("消息'%s'发送失败！错误代码是%d，错误信息是'%s'\n", \
				buf, errno, strerror(errno)); \
		else \
		printf("消息'%s'发送成功，共发送了%d个字节！\n", \
				buf, n)

#define logr(n,buf)	if (n > 0) \
	printf("接收消息成功:'%s'，共%d个字节的数据\n", \
			buf, n); \
		else \
		printf("消息接收失败！错误代码是%d，错误信息是'%s'\n", \
				errno, strerror(errno))

char buf[N + 1] = {0};
char filename[64] = {0};
unsigned int n;

void updata(SSL * ssl)
{
	int fd;
	long int file_len;
uploop:
	n = SSL_read(ssl,&buf[0],1);
	printf("文件名长度是: %d\n",buf[0]);
	n = SSL_read(ssl,filename,buf[0]);
	logr(n,filename);
	n = SSL_read(ssl,&file_len,4); 
	if(file_len <= 0)
	{
		printf("文件大小不对，重新等待客户端输入:\n");
		goto uploop;
	}
	printf("文件大小是: %ld\n",file_len);

	fd = open(filename,O_RDWR | O_CREAT | O_TRUNC, 0664);
	while((n = SSL_read(ssl,buf,N)) > 0)
	{
		write(fd,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n; 
		if(file_len <= 0)
			break;
	}
	printf("\n");
	if(file_len == 0)
		printf("上传成功...\n");
	else
		printf("上传失败...\n");
	close(fd);
}

void download(SSL * ssl)
{
	int fd;
	struct stat st;
	long int file_len;
dloop:
	n = SSL_read(ssl,&buf[0],1);
	printf("文件名长度是: %d\n",buf[0]);
	n = SSL_read(ssl,filename,buf[0]);
	logr(n,filename);
	stat(filename,&st);
	file_len = st.st_size;
	n = SSL_write(ssl,&file_len,4);
	if(file_len <= 0)
	{
		printf("文件大小不对，重新等待客户端输入:\n");
		goto dloop;
	}
	printf("文件大小是: %ld\n",file_len);

	fd = open(filename,O_RDONLY);
	while((n = read(fd,buf,N)) > 0)
	{
		SSL_write(ssl,buf,n);
		printf(".");
		fflush(NULL);
		file_len -= n;
		if(file_len <= 0)
			break;
	}
	printf("\n");
	if(file_len == 0)
		printf("下载成功...\n");
	else
		printf("下载失败...\n");

	close(fd);
}

void handler(char cmd,SSL * ssl)
{
	switch(cmd)
	{
	case 'U':
		printf("正在从客户端上传...\n");
		updata(ssl);
		break;
	case 'D':
		printf("正在从服务器下载\n");
		download(ssl);
		break;
	default:
		printf("接收的命令有误!\n");
	}
	bzero(buf,N+1); 
	bzero(filename,64); 
}

int main(int argc, const char *argv[])
{
	int sockfd, connfd;
	struct sockaddr_in  serveraddr, clientaddr;
	socklen_t addrlen;
	unsigned int n, myport, lisnum = 5;
	SSL_CTX *ctx;

	if(argc < 3)
	{
		fprintf(stderr,"Usage:%s ipaddr port\n",argv[0]);
		exit(1);
	}

	/* SSL 库初始化 */
	SSL_library_init();
	/* 载入所有 SSL 算法 */
	OpenSSL_add_all_algorithms();
	/* 载入所有 SSL 错误消息 */
	SSL_load_error_strings();
	/* 以 SSL V2 和 V3 标准兼容方式产生一个 SSL_CTX ，即 SSL
	 * Content Text */
	ctx = SSL_CTX_new(SSLv23_server_method());
	/* 也可以用 SSLv2_server_method() 或
	 * SSLv3_server_method() 单独表示 V2 或 V3标准 */
	if (ctx == NULL) {
		ERR_print_errors_fp(stdout);
		exit(1);
	}
	/* 载入用户的数字证书， 此证书用来发送给客户端。
	 * 证书里包含有公钥 */
	if (SSL_CTX_use_certificate_file(ctx, "./cacert.pem", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stdout);
		exit(1);
	}
	/* 载入用户私钥 */
	if (SSL_CTX_use_PrivateKey_file(ctx, "./privkey.pem", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stdout);
		exit(1);
	}
	/* 检查用户私钥是否正确 */
	if (!SSL_CTX_check_private_key(ctx)) {
		ERR_print_errors_fp(stdout);
		exit(1);
	}
	//1.建立连接
	sockfd = socket(AF_INET , SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("fail to socket");
		exit(1);
	} else
		printf("socket created\n");

	bzero(&serveraddr, sizeof(serveraddr));
	if (argv[1])
		serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	else
		serveraddr.sin_addr.s_addr = INADDR_ANY;

	if (argv[2])
		myport = atoi(argv[2]);
	else
		myport = 7838;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(myport);
	if(bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		perror("fail to bind");
		exit(1);
	} else
		printf("binded\n");

	if(listen(sockfd, lisnum) == -1)
	{
		perror("fail to listen");
		exit(1);
	} else
		printf("begin listen\n");

	addrlen = sizeof(clientaddr);

	while(1)  //----->while(1)可以响应多个客户端
	{	
		SSL *ssl;

		connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &addrlen);
		if(connfd == -1)
		{
			perror("fail to accept");
			exit(errno);
		} else
			printf("server: got connection from %s, port %d, socket %d\n",
					(char *)inet_ntoa(clientaddr.sin_addr),
					ntohs(clientaddr.sin_port), connfd);

		/* 基于 ctx 产生一个新的 SSL */
		ssl = SSL_new(ctx);
		/* 将连接用户的 socket 加入到 SSL */
		SSL_set_fd(ssl, connfd);
		/* 建立 SSL 连接 */
		if (SSL_accept(ssl) == -1) {
			perror("accept");
			close(connfd);
			break;
		}

		//2.响应客户端的请求
		while(1)  //----->while(1)可以响应一个客户端的多次请求
		{	
			/* 开始处理每个新连接上的数据收发 */
			//(1).读取命令
			bzero(buf,N + 1);
			n = SSL_read(ssl, &buf[0], 1);
			logr(n,&buf[0]);
			if (n != 1)
				break;
			//(2).执行命令
			if(buf[0] == 'Q')
				goto finish;
			else
				handler(buf[0],ssl);
		}
		/* 处理每个新连接上的数据收发结束 */
finish:
		/* 关闭 SSL 连接 */
		SSL_shutdown(ssl);
		/* 释放 SSL */
		SSL_free(ssl);
		/* 关闭 socket */
		close(connfd);
		printf("客户端退出!\n");

	}
	//3.关闭连接
	/* 关闭监听的 socket */
	close(sockfd);
	/* 释放 CTX */
	SSL_CTX_free(ctx);
	return 0;
}

