#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
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

void ShowCerts(SSL * ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl);
	if (cert != NULL) {
		printf("数字证书信息:\n");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		printf("证书: %s\n", line);
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		printf("颁发者: %s\n", line);
		free(line);
		X509_free(cert);
	} else
		printf("无证书信息!\n");
}

void updata(SSL * ssl)
{
	int fd;
	struct stat st;
	long int file_len;

	//(1).发送上传命令
	buf[0] = 'U';
	n = SSL_write(ssl,&buf[0],1);
	logs(n,&buf[0]);
uploop:
	//(2.1).发送文件名长度
	printf("请输入要上传的文件名:\n");
	scanf("%s",buf);
	strcpy(filename,buf);
	buf[0] = strlen(filename);
	n = SSL_write(ssl,&buf[0],1);
	printf("文件名长度是: %d\n",buf[0]);
	//(2.2).发送文件名
	n = SSL_write(ssl,filename,buf[0]);
	logs(n,filename);
	//(3).发送文件长度
	stat(filename,&st);
	file_len = st.st_size;
	n = SSL_write(ssl,&file_len,4);
	if(file_len <= 0)
	{
		printf("输入的文件名有错！\n");
		goto uploop;
	}
	printf("文件大小是: %ld\n",file_len);
	//(4).发送文件内容
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
		printf("上传成功...\n");
	else
		printf("上传失败...\n");
	close(fd);
}
void download(SSL * ssl)
{
	int fd;
	long int file_len;

	//(1).发送下载命令
	buf[0] = 'D';
	n = SSL_write(ssl,&buf[0],1);
	logs(n,&buf[0]);
dloop:
	//(2.1).发送文件名长度
	printf("请输入要下载的文件名:\n");
	scanf("%s",buf);
	strcpy(filename,buf);
	buf[0] = strlen(filename);
	n = SSL_write(ssl,&buf[0],1);
	printf("文件名长度是: %d\n",buf[0]);
	//(2.2).发送文件名
	n = SSL_write(ssl,filename,buf[0]);
	logs(n,filename);
	//(3).接收文件长度
	n = SSL_read(ssl,&file_len,4);
	if(file_len <= 0)
	{
		printf("输入的文件名有错！\n");
		goto dloop;
	}
	printf("文件大小是: %ld\n",file_len);
	//(4).接收文件内容
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
		printf("下载成功...\n");
	else
		printf("下载失败...\n");
	close(fd);
}
void menu(SSL * ssl)
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
			updata(ssl);
			break;
		case 2:
			printf("正在下载...\n");
			download(ssl);
			break;
		case 3:
			system("clear");
			printf("退出...\n");
			goto end;
		default :
			printf("请按照提示输入正确的选项!\n");
		}
		bzero(buf,N+1); 
		bzero(filename,64); 
	}
end:
	buf[0] = 'Q';
	n = SSL_write(ssl, &buf[0], 1);
	logs(n,&buf[0]);
}

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr;
	SSL_CTX *ctx;
	SSL *ssl;

	if(argc < 3)
	{
		fprintf(stderr,"Usage:%s ipaddr port\n",argv[0]);
		exit(1);
	}
	//1.建立连接
	/* SSL 库初始化，参看 ssl-server.c 代码 */
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv23_client_method());
	if (ctx == NULL) {
		ERR_print_errors_fp(stdout);
		exit(1);
	}
	/* 创建一个 socket 用于 tcp 通信 */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("fail to socket");
		exit(errno);
	}
	printf("socket created\n");

	/* 初始化服务器端（对方）的地址和端口信息 */
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	if (inet_aton(argv[1], (struct in_addr *) &serveraddr.sin_addr.s_addr) == 0) {
		perror(argv[1]);
		exit(errno);
	}
	printf("address created\n");

	if(connect(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) !=  0)
	{
		perror("fail to connect");
		exit(errno);
	}
	printf("server connected\n");

	/* 基于 ctx 产生一个新的 SSL */
	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, sockfd);
	/* 建立 SSL 连接 */
	if (SSL_connect(ssl) == -1)
		ERR_print_errors_fp(stderr);
	else {
		printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
		ShowCerts(ssl);
	}
	//2.实现上传和下载的菜单
	menu(ssl);
	//3.关闭连接
finish:
	/* 关闭连接 */
	SSL_shutdown(ssl);
	SSL_free(ssl);
	close(sockfd);
	SSL_CTX_free(ctx);
	return 0;
}

