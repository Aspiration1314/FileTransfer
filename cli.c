#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<sys/types.h>

int exist_file(char *file, char *name)//BF算法
{
	assert(file != NULL && name != NULL);
	int lens = strlen(file);
	int lenp = strlen(name);
	if (lens<lenp)
	{
		return -1;
	}
	int i = 0, j = 0;
	while (i<lens&&j<lenp)
	{
		if (file[i] == name[j])
		{
			i++;
			j++;
		}
		else
		{
			i = i - j + 1;
			j = 0;
		}
	}
	if (j >= lenp && file[i] == '\n')
	{

		return 1;
	}
	return -1;
}

int compare_md5(int sockfd, char *name)
{
	char ser_md5[33] = { 0 };
	recv(sockfd, ser_md5, 32, 0);//收到客户端发过来的md5值
	printf("ser_md5=%s\n", ser_md5);
	int pipefd[2] = { 0 };
	pipe(pipefd);
	pid_t pid = fork();
	if (pid == 0)
	{
		close(pipefd[0]);
		dup2(pipefd[1], 1);
		dup2(pipefd[1], 2);
		char *myargv[5] = { "md5sum", name };
		execvp("md5sum", myargv);
		exit(0);
	}
	wait(NULL);
	close(pipefd[1]);
	char cli_md5[33] = { 0 };
	read(pipefd[0], cli_md5, 32);
	printf("cli_md5=%s\n", cli_md5);
	close(pipefd[0]);
	int i = 0;
	for (; i<32; i++)
	{
		if (cli_md5[i] != ser_md5[i])
		{
			printf("download error!");
			return 0;
		}
	}
	printf("download succeed\n\n");
}


void rm_already(char *name)
{
	int pipefd[2] = { 0 };
	pipe(pipefd);
	pid_t pid = fork();
	if (pid == 0)
	{
		close(pipefd[0]);
		dup2(pipefd[1], 1);
		dup2(pipefd[1], 2);
		char *myargv[5] = { "rm", name };
		execvp("rm", myargv);
		exit(0);
	}
	wait(NULL);
	close(pipefd[1]);
	close(pipefd[0]);
	printf("already exist delete\n\n");
}

void printls(char *read_buff)//使得ls显示适应窗口大小
{
	int maxsize = 0;
	int i = 0;
	int count = 0;
	int t = 0;
	while (read_buff[i] != '\0')
	{
		if (read_buff[i] == '\n')
		{
			t += 1;
			if (count>maxsize)
			{
				maxsize = count;
			}
			count = 0;
			i++;
			continue;
		}
		i++;
		count++;
	}
	struct winsize size;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &size);
	int num = (size.ws_col) / (maxsize + 3);
	i = 3;
	while (1)
	{
		int j = 0;
		for (; j<num; j++)
		{
			int sign = maxsize;
			while (read_buff[i] != '\n')
			{
				printf("%c", read_buff[i]);
				i++;
				sign--;
			}
			while (sign != 0)
			{
				sign--;
				printf(" ");
			}
			i++;
			printf("   ");
			if (read_buff[i] == '\0')
			{
				printf("\n");
				return;
			}
		}
		printf("\n");
	}
}
//带返回值，因为ser有可能关闭连接
int recv_file(int sockfd, char* name)
{
	char buff[128] = { 0 };
	if (recv(sockfd, buff, 127, 0) <= 0)//连接断开，返回失败，退出
	{
		return -1;
	}
	if (strncmp(buff, "ok", 2) != 0)//buff的前两个字符不是ok，说明ser告诉我们不能下载
	{
		printf("%s\n", buff);
		return 0;//可以理解为连接成功了，但是不存在，不能下载，需要重新输入新的命令
	}


	//如果已经存在name.tmp，则直接接收数据，如果不存在，则需要新建文件夹

	char *p = (char *)malloc(sizeof(char)*(strlen(name) + 5));//新建临时文件
	strcpy(p, name);
	strcat(p, ".tmp");


	int pi[2];
	pipe(pi);
	pid_t pid = fork();
	char find_buff[1024] = { 0 };
	if (pid == 0)
	{
		close(pi[0]);
		dup2(pi[1], 1);
		dup2(pi[1], 2);
		execl("/bin/ls", "ls", NULL);
		perror("execl error");
		close(pi[1]);
	}
	else
	{
		close(pi[1]);
		wait(NULL);
		read(pi[0], find_buff, 1000);
		close(pi[0]);
	}


	if (exist_file(find_buff, name) == 1)
	{
		printf("该文件已存在\n");
		send(sockfd, "yes", 3, 0);
		return 1;
	}
	else
	{
		send(sockfd, "no", 2, 0);
	}

	//拿到ok#后的文件大小，继续执行交互    
	int size = 0;
	sscanf(buff + 3, "%d", &size);//得到文件大小，以%d的形式转到size中
	printf("file(%s):%d\n", name, size);

	int fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0600);//在本地以只写方式打开文件，没有就创建
	if (fd == -1)
	{
		send(sockfd, "err", 3, 0);
	}
	int hsize = lseek(fd, 0, SEEK_END);//计算已有文件大小
	lseek(fd, 0, SEEK_SET);

	char send_hsize[30] = { 0 };
	sprintf(send_hsize, "ok#%d", hsize);
	printf("hsize=%d\n", hsize);
	send(sockfd, send_hsize, strlen(send_hsize), 0);//发送已有文件大小



	int num = 0;
	int cur_size = 0;//当前大小
	if (exist_file(find_buff, p) == 1)
	{
		cur_size = hsize;
	}

	char data[256] = { 0 };
	float f = 0;

	if (cur_size != 0)//选择是否启动断点续传
	{
		f = cur_size*100.0 / size;
		printf("当前已下载：%.2f%%\n", f);
		printf("是否启动断点续传（是：y 否：n）:\n");
		char qbuff[128] = { 0 };
		fgets(qbuff, 127, stdin);
		qbuff[strlen(buff) - 1] = 0;
		send(sockfd, qbuff, 127, 0);

		if (strncmp(qbuff, "y", 1) == 0)//启动
		{
			int now = lseek(fd, hsize, SEEK_SET);
			while (1)
			{
				num = recv(sockfd, data, 256, 0);
				if (num <= 0)
				{
					return -1;
				}
				write(fd, data, num);
				cur_size = cur_size + num;
				f = cur_size*100.0 / size;
				printf("continue:%.2f%%\r\033[?25l", f);
				fflush(stdout);
				if (cur_size >= size)
				{
					printf("\n");
					break;
				}
			}

			rename(p, name);
			compare_md5(sockfd, name);
			return 0;
		}

		if (strncmp(qbuff, "n", 1) == 0)
		{
			rm_already(p);
			return 0;
		}

		else//不启动
		{
			return -1;
		}

	}

	else
	{
		while (1)
		{
			num = recv(sockfd, data, 256, 0);//recv接收放到data里
			if (num <= 0)//对方关闭
			{
				return -1;
			}
			write(fd, data, num);
			cur_size = cur_size + num;//累加
			float f = cur_size*100.0 / size;//打印下载进度百分比
			printf("download:%.2f%%\r\033[?25l", f);
			fflush(stdout);
			if (cur_size >= size)
			{
				break;
			}
		}

		rename(p, name);
		compare_md5(sockfd, name);

		return 0;
	}

}

void send_file(int sockfd, char* myargv[])
{
	if (myargv[1] == NULL)
	{
		printf("upload:no file\n");
		return;
	}
	int fd = open(myargv[1], O_RDONLY);
	if (fd == -1)
	{
		printf("not found!\n");
		return;
	}
	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	char status[32] = { 0 };
	sprintf(status, "ok#%d", size);
	send(sockfd, status, strlen(status), 0);
	char ser_status[32] = { 0 };
	printf("file(%s):%d\n", myargv[1], size);
	if (recv(sockfd, ser_status, 31, 0) <= 0)
	{
		return;
	}
	if (strcmp(ser_status, "ok") != 0)
	{
		return;
	}
	char data[256] = { 0 };
	int num = 0;
	int cur_size = 0;
	while ((num = read(fd, data, 256))>0)
	{
		cur_size = cur_size + num;
		float f = cur_size*100.0 / size;
		printf("upload:%.2f%%\r\033[?25l", f);
		fflush(stdout);
		send(sockfd, data, num, 0);
	}
	printf("upload succeed\n\n");
	close(fd);
	return;
}


int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd != -1);

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(6000);
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int res = connect(sockfd, (struct sockaddr*)&saddr, sizeof(saddr));
	assert(res != -1);

	while (1)
	{
		char buff[128] = { 0 };
		printf("you've connect,please input:");
		fflush(stdout);
		fgets(buff, 128, stdin);
		if (strncmp(buff, "end", 3) == 0)
		{
			break;
		}
		buff[strlen(buff) - 1] = 0;
		if (buff[0] == 0)
		{
			continue;
		}
		char tmp[128] = { 0 };
		//把buff复制一份出来，存到tmp里，原来buff里的内容就不变了，保证buff的完整性
		strcpy(tmp, buff);
		char* myargv[10] = { 0 };
		char* s = strtok(tmp, " ");
		int i = 0;
		while (s != NULL)
		{
			myargv[i++] = s;
			s = strtok(NULL, " ");
		}
		if (strcmp(myargv[0], "get") == 0)
		{
			send(sockfd, buff, strlen(buff), 0);
			recv_file(sockfd, myargv[1]);
		}
		else if (strcmp(myargv[0], "put") == 0)
		{
			send(sockfd, buff, strlen(buff), 0);
			send_file(sockfd, myargv);
		}
		else if (strcmp(myargv[0], "local") == 0 && strcmp(myargv[1], "ls") == 0)
		{
			pid_t pid = fork();
			if (pid == 0)
			{
				printf("local:\n");
				execl("/bin/ls", "ls", NULL);
			}
			wait(NULL);
		}
		else
		{
			send(sockfd, buff, strlen(buff), 0);
			char read_buff[1024] = { 0 };
			recv(sockfd, read_buff, 1023, 0);
			printls(read_buff);
		}
	}
	close(sockfd);
	exit(0);
}
