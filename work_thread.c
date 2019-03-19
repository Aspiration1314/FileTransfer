#include "work_thread.h"
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#define  ARGC   10

void send_md5(int c,char *myargv[])
{
    //创建管道，fork+exec 执行md5sum 得到文件的md5值
    int pipefd[2]={0};
    pipe(pipefd);
    pid_t pid=fork();
    if(pid==0)
    {
        close(pipefd[0]);
        dup2(pipefd[1],1);
        dup2(pipefd[1],2);
        execvp("md5sum",myargv);
    }
    wait(NULL);
    close(pipefd[1]);
    char md5[33]={0};
    read(pipefd[0],md5,32);
    close(pipefd[0]);
    send(c,md5,32,0);//将md5值发给cli

}

/*
 *启动线程
 */ 
void thread_start( int c)
{
    pthread_t id;
    pthread_create(&id,NULL,work_thread,(void*)c);
}
//分割字符串
void get_argv(char buff[],char* myargv[])
{
    char *p = NULL;  
    char *s = strtok_r(buff, " ",&p); 
    int i = 0;
    while(s != NULL)
    {
        myargv[i++] = s;
        s=strtok_r(NULL," ",&p);
    }
}
void send_file(int c, char* myargv[])
{
    if(myargv[1]==NULL)//看是否有该文件
    {
        send(c,"get:no file name!",17,0);
        return;
    }
    int fd = open(myargv[1],O_RDONLY);
    if(fd == -1)//看文件是否能打开
    {
        send(c,"not found!",10,0);
        return;
    }
    //成功则告诉cli文件大小
    int size = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);
    char status[32]={0};
    sprintf(status,"ok#%d",size);
    send(c,status,strlen(status),0);//给cli发送状态信息

    char has[4]={0};
    recv(c,has,4,0);
    if(strncmp(has,"yes",3)==0)
    {
        return;
    }
       
    char recv_hsize[30]={0};
    //recv  cli 发过来的信息
    if(recv(c,recv_hsize,30,0)<=0)//说明recv没有回复，不进行下载
    {
        return;
    }
    if(strncmp(recv_hsize,"ok",2)!=0)//收到ok,cli想下载
    {
        return;
    }
    int hsize=0;
    sscanf(recv_hsize+3,"%d",&hsize);
    
    if(hsize!=0)
    {
        char qbuff[128]={0};
        recv(c,qbuff,128,0);
        
        if(strncmp(qbuff,"y",1)==0)
        {
            
            char data[256]={0};
            int num;
            int now=lseek(fd,hsize,SEEK_SET);
            while((num=read(fd,data,256))>0)
            {
                send(c,data,num,0);
            }
            
            send_md5(c,myargv);
            close(fd);
            return;
        }
        else
        {
            return;
        }
        
    }


    else
    {
        char data[256]={0};
        int num=0;
        int ctrl_c=0;
        while((num=read(fd,data,256))>0)//从open的那个文件里读数据，读完放到data里
        {
            if((ctrl_c=send(c,data,num,0))<0)
            {
                signal(SIGPIPE,SIG_IGN);
                return;
            }
            //给cli发读到的文件内容
        }
        send_md5(c,myargv);
        close(fd);
        return;
    }
    
}
int recv_file(int c, char* name)
{
	char buff[128] = { 0 };
	if (recv(c, buff, 127, 0) <= 0)
	{
		return -1;
	}
	if (strncmp(buff, "ok", 2) != 0)
	{
		printf("%s\n", buff);
		return 0;
	}
	int size = 0;
	sscanf(buff + 3, "%d", &size);
	int fd = open(name, O_WRONLY | O_CREAT, 0600);
	if (fd == -1)
	{
		send(c, "err", 3, 0);
		return;
	}
	send(c, "ok", 2, 0);
	int num = 0;
	int cur_size = 0;
	char data[256] = { 0 };
	while (1)
	{
		num = recv(c, data, 256, 0);
		if (num <= 0)
		{
			return -1;
		}
		write(fd, data, num);
		cur_size = cur_size + num;
		fflush(stdout);
		if (cur_size >= size)
		{
			break;
		}
	}
	return 0;
}

/*
 *工作线程
 */
void* work_thread(void * arg)
{
    int c = (int)arg;

    //测试
    while( 1 )
    {
        char buff[128] = {0};
        
        int n = recv(c,buff,127,0);//touch file, rm a.c , ls,
        if ( n <= 0 )
        {
            close(c);
            printf("one client over\n");
            break;
        }
        printf("recv:%s\n",buff);
        char * myargv[ARGC] = {0};//定义数组传给分割函数
        get_argv(buff,myargv);
        if(strcmp(myargv[0],"get")==0)
        {
            send_file(c,myargv);
        }
        else if(strcmp(myargv[0],"put")==0)
        {
            recv_file(c,myargv[1]);	
        }
        else
        {
            int pipefd[2];
            pipe(pipefd);
            pid_t pid = fork();
            if ( pid == 0 )
            {
                dup2(pipefd[1],1);
                dup2(pipefd[1],2);
                execvp(myargv[0],myargv);
                perror("execvp error");
                exit(0);
            }
            close(pipefd[1]);
            wait(NULL);
            char read_buff[1024]={0};
            strcpy(read_buff,"ok#");
            read(pipefd[0],read_buff+strlen(read_buff),1000);
            send(c,read_buff,strlen(read_buff),0);
            close(pipefd[0]);
        }
    }
}
