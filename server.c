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
#include <pthread.h>

#define N 1024

#define logs(n,buf)	if (n < 0) \
	printf("消息'%s'发送失败！错误代码是%d，错误信息是'%s'\n", \
			buf, errno, strerror(errno)); \
else \
printf("消息'%s'发送成功，共发送了%d个字节！\n", \
		buf, n)

#define logr(n,buf)	if (n > 0) \
	printf("接收消息成功:'%s'，共%d个字节的数据\n", \
			buf, n)

SSL_CTX *ctx;

typedef struct {
	unsigned int n;
	int connfd;
	struct sockaddr_in clientaddr;
	char buf[N + 1];
	char filename[64];
	int err_count;
} Data;

typedef struct task 
{ 
	void *(*process) (Data * d); 
	Data * d;
	struct task *next; 
} Cthread_task; 

/*线程池结构*/ 
typedef struct 
{ 
	pthread_mutex_t queue_lock; 
	pthread_cond_t queue_ready; 

	/*链表结构，线程池中所有等待任务*/ 
	Cthread_task *queue_head; 

	/*是否销毁线程池*/ 
	int shutdown; 
	pthread_t *threadid; 

	/*线程池中线程数目*/ 
	int max_thread_num; 

	/*当前等待的任务数*/ 
	int cur_task_size; 

} Cthread_pool; 

Cthread_pool *pool = NULL; 

void * thread_routine (void *arg) 
{ 
	printf ("starting thread %#lx\n", pthread_self ()); 
	while (1) 
	{ 
		pthread_mutex_lock (&(pool->queue_lock)); 

		while (pool->cur_task_size == 0 && !pool->shutdown) 
		{ 
			printf ("thread %#lx is waiting\n", pthread_self ()); 
			pthread_cond_wait (&(pool->queue_ready), &(pool->queue_lock)); 
		} 

		/*线程池要销毁了*/ 
		if (pool->shutdown) 
		{ 
			/*遇到break,continue,return等跳转语句，千万不要忘记先解锁*/ 
			pthread_mutex_unlock (&(pool->queue_lock)); 
			printf ("thread %#lx will exit\n", pthread_self ()); 
			pthread_exit (NULL); 
		} 

		printf ("thread %#lx is starting to work\n", pthread_self ()); 


		/*待处理任务减1，并取出链表中的头元素*/ 
		pool->cur_task_size--; 
		Cthread_task *task = pool->queue_head; 
		pool->queue_head = task->next; 
		pthread_mutex_unlock (&(pool->queue_lock)); 

		/*调用回调函数，执行任务*/ 
		(*(task->process)) (task->d); 
		free (task); 
		task = NULL; 
	} 
	/*这一句应该是不可达的*/ 
	pthread_exit (NULL); 
}

void pool_init (int max_thread_num) 
{ 
	int i = 0;

	pool = (Cthread_pool *) malloc (sizeof (Cthread_pool)); 

	pthread_mutex_init (&(pool->queue_lock), NULL); 
	/*初始化条件变量*/
	pthread_cond_init (&(pool->queue_ready), NULL); 

	pool->queue_head = NULL; 

	pool->max_thread_num = max_thread_num; 
	pool->cur_task_size = 0; 

	pool->shutdown = 0; 

	pool->threadid = (pthread_t *) malloc (max_thread_num * sizeof (pthread_t)); 

	for (i = 0; i < max_thread_num; i++) 
	{  
		pthread_create (&(pool->threadid[i]), NULL, thread_routine, NULL); 
	} 
} 

int pool_add_task (void *(*process) (Data * d), Data * d) 
{ 
	/*构造一个新任务*/ 
	Cthread_task *task = (Cthread_task *) malloc (sizeof (Cthread_task)); 
	task->process = process; 
	task->d = d; 
	task->next = NULL;

	pthread_mutex_lock (&(pool->queue_lock)); 
	/*将任务加入到等待队列中*/ 
	Cthread_task *member = pool->queue_head; 
	if (member != NULL) 
	{ 
		while (member->next != NULL) 
			member = member->next; 
		member->next = task; 
	} 
	else 
	{ 
		pool->queue_head = task; 
	} 

	pool->cur_task_size++; 
	pthread_mutex_unlock (&(pool->queue_lock)); 

	pthread_cond_signal (&(pool->queue_ready)); 

	return 0; 
} 



void updata(SSL * ssl,Data * d)
{
	int fd;
	long int file_len;
	d->err_count = 3;
	d->n = SSL_read(ssl,d->buf,1);
	if(d->n <= 0) return;
	printf("文件名长度是: %d\n",d->buf[0]);
	d->n = SSL_read(ssl,d->filename,d->buf[0]);
	if(d->n <= 0) return;
	logr(d->n,d->filename);
	d->n = SSL_read(ssl,&file_len,4); 
	if(d->n <= 0) return;
	if(file_len <= 0) return;
	printf("文件大小是: %ld\n",file_len);

	fd = open(d->filename,O_RDWR | O_CREAT | O_TRUNC, 0664);
	while((d->n = SSL_read(ssl,d->buf,N)) > 0)
	{
		write(fd,d->buf,d->n);
		printf(".");
		fflush(NULL);
		file_len -= d->n; 
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

void download(SSL * ssl,Data * d)
{
	int fd;
	struct stat st;
	long int file_len;
	d->err_count = 3;
dloop:
	d->n = SSL_read(ssl,d->buf,1);
	if(d->n <= 0) return;
	printf("文件名长度是: %d\n",d->buf[0]);
	d->n = SSL_read(ssl,d->filename,d->buf[0]);
	logr(d->n,d->filename);
	if(d->n <= 0) return;
	stat(d->filename,&st);
	file_len = st.st_size;
	d->n = SSL_write(ssl,&file_len,4);
	if(file_len <= 0)
	{
		d->err_count--;
		if(d->err_count < 0) return;
		printf("文件大小不对，重新等待客户端输入:\n");
		goto dloop;
	}
	printf("文件大小是: %ld\n",file_len);

	fd = open(d->filename,O_RDONLY);
	while((d->n = read(fd,d->buf,N)) > 0)
	{
		SSL_write(ssl,d->buf,d->n);
		printf(".");
		fflush(NULL);
		file_len -= d->n;
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

void handler(char cmd,SSL * ssl,Data * d)
{
	switch(cmd)
	{
	case 'U':
		printf("正在从客户端上传...\n");
		updata(ssl,d);
		break;
	case 'D':
		printf("正在从服务器下载\n");
		download(ssl,d);
		break;
	default:
		printf("接收的命令有误!\n");
	}
	bzero(d,sizeof(Data)); 
}

void * process (Data * d) 
{ 
	SSL *ssl;

	printf("server: got connection from %s, port %d, socket %d\n",
			(char *)inet_ntoa(d->clientaddr.sin_addr),
			ntohs(d->clientaddr.sin_port), d->connfd);

	/* 基于 ctx 产生一个新的 SSL */
	ssl = SSL_new(ctx);
	/* 将连接用户的 socket 加入到 SSL */
	SSL_set_fd(ssl, d->connfd);
	/* 建立 SSL 连接 */
	if (SSL_accept(ssl) == -1) {
		perror("accept");
		goto fail;
	}
	//2.响应客户端的请求
	while(1)  //----->while(1)可以响应一个客户端的多次请求
	{	
		/* 开始处理每个新连接上的数据收发 */
		//(1).读取命令
		bzero(d->buf,N + 1);
		d->n = SSL_read(ssl, &(d->buf), 1);
		logr(d->n,d->buf);
		if (d->n != 1)
			break;
		//(2).执行命令
		if(d->buf[0] == 'Q')
			goto finish;
		else
			handler(d->buf[0],ssl,d);
	}
	/* 处理每个新连接上的数据收发结束 */
finish:
	/* 关闭 SSL 连接 */
	SSL_shutdown(ssl);
	/* 释放 SSL */
	SSL_free(ssl);
fail:
	/* 关闭 socket */
	close(d->connfd);
	printf("客户端退出!\n");

	return NULL; 
}

int main(int argc, const char *argv[])
{
	int sockfd, connfd;
	struct sockaddr_in  serveraddr;
	socklen_t addrlen;
	unsigned int n, myport, lisnum = 5;

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

	addrlen = sizeof(struct sockaddr_in);

	//p1. 初始化线程池
	pool_init(5);

	while(1)  //----->while(1)可以响应多个客户端
	{	
		Data * d = malloc(sizeof(Data));
		if(d == NULL)
		{
			perror("malloc");
			exit(errno);
		}
		bzero(d,sizeof(Data));
		d->connfd = accept(sockfd, (struct sockaddr *)&(d->clientaddr), &addrlen);
		if(d->connfd == -1)
		{
			perror("fail to accept");
			exit(errno);
		}
		//p2. 执行process，将process任务交给线程池
		pool_add_task(process,d);

	}
	//3.关闭连接
	/* 关闭监听的 socket */
	close(sockfd);
	/* 释放 CTX */
	SSL_CTX_free(ctx);
	return 0;
}

