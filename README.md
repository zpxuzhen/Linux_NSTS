# NSTS
网络安全传输系统

Sprint 1 - 传输子系统设计
Step1. 系统框架搭建

客户端:
	1.建立连接
	2.实现上传下载功能的菜单
		---->上传:命令Q
				  文件名
				  文件内容(先发送文件长度)
		---->下载:命令D
				  文件名
				  文件内容(先接收文件长度)
		---->退出:命令Q
				  清理屏幕
				  break;
	3.关闭连接

服务器:
	1.建立连接
		--->1.1.创建socket
		--->1.2.绑定地址
		--->1.3.监听
	while(1)
	{
		--->1.4.等待连接
	2.响应客户端的请求
		while(1)
		{
		--->2.1.读取命令
		--->2.2.处理请求
			--->'Q':
				关闭连接connfd
				break;
			--->'其他':
				handler:----------->上传与下载
		}
	}
	3.关闭连接sockfd



















