#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
/*
设置网卡的IP地址
$ sudo ifconfig eth0 192.168.1.23  

服务器创建流程:
1. 创建socket套接字(文件描述符)---类型open函数一样
2. 绑定端口号(创建服务器:提供端口号和IP地址)
3. 设置监听的客户端数量(设置待处理的队列最大缓存待连接的客户端数量)
4. 等待客户端连接(被动--阻塞): 多线程和多进程方式并发处理客户端连接。
5. 实现通信(客户端连接成功)
*/

int server_sockfd=0;
void *tid_func(void *arg)
{
    int *tmp_fd=(int *)arg;
    int client_sockfd=*tmp_fd;//取出套接字
    free(tmp_fd);

    int select_cnt=0;
    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;
    int rx_cnt=1;
    char rx_buff[100];
    char wx_buff[1024];
    //与客户端通信
    while(1)
    {
        timeout.tv_sec=10;
        timeout.tv_usec=1000;
        //清空事件集合
        FD_ZERO(&readfds);
        //添加要检测的文件描述符
        FD_SET(client_sockfd,&readfds);
        select_cnt=select(client_sockfd+1,&readfds,&writefds,NULL,&timeout);
        if(select_cnt>0)
        {
            //读客户端发来的数据
            rx_cnt=read(client_sockfd,rx_buff,sizeof(rx_buff)-1);
            if(rx_cnt==0)
            {
                break;
            }
            rx_buff[rx_cnt]='\0';
            snprintf(wx_buff,sizeof(wx_buff),"server data:%s\n",rx_buff);
            if(write(client_sockfd,wx_buff,strlen(wx_buff))<=0)
            {
                printf("向客户端发送失败\n");
                break;
            }
        }
        else if(select_cnt==0)
        {
            printf("%d客户端10秒没有向客户端发送任何数据.\n",client_sockfd);
            break;
        }
        else
        {
            printf("select函数错误\n");
            break;
        }    
    }
    printf("%d 客户端断开连接.\n",client_sockfd);
    pthread_exit(NULL);
}
void signal_func(int sig)
{
    close(server_sockfd);
    exit(0);
}
int main(int argc,char **argv)
{
    if(argc!=2)
    {
        printf("正确格式为:./a.out <port>\n");
        return 0;
    }
    //注册需要捕获的信号
    signal(SIGINT,signal_func);
    signal(SIGPIPE,signal_func);

    //创建socket套接字(文件描述符)
    server_sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(socket<0)
    {
        printf("服务器套接字创建失败.\n");
        return 0;
    }

    //绑定端口号(创建服务器:提供端口号和IP地址)
    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr=INADDR_ANY;
    //server_addr.sin_addr.s_addr=inet_addr("192.168.43.1");
    if(bind(server_sockfd,(const struct sockaddr *)&server_addr,sizeof(struct sockaddr)))
    {
        printf("创建端口号失败\n");
        return 0;
    }

    //设置等待连接的客户端数量
    if(listen(server_sockfd,100))
    {
       printf("设置等待的客户端数量失败\n");
        return 0;
    }
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    uint16_t port_cnt=0;
    time_t tim=time(NULL);
    int *client_sockfd=NULL;
    pthread_t tid=0;
    pthread_t tid_w=0;
    if(pthread_create(&tid_w,NULL,tid_w))
    while(1)
    {
        //打开日志文件(记录连接的客户端端口号和IP)
        FILE *fp_ipport=fopen("log.txt","a+b");

        client_sockfd=malloc(sizeof(int));
        addrlen=sizeof(struct sockaddr);
        *client_sockfd=accept(server_sockfd,(struct sockaddr *)&client_addr,&addrlen);
        if(client_sockfd<0)
        {
            printf("服务器:客户端连接失败.\n");
            break;
        }
        //写日志
        port_cnt=ntohs(client_addr.sin_port);
        fprintf(fp_ipport,"客户端端口号:%d 客户端IP:%s %s\n",port_cnt,inet_ntoa(client_addr.sin_addr),ctime(&tim));
        fclose(fp_ipport);
        //创线程
        if(pthread_create(&tid,NULL,tid_func,client_sockfd))
        {
            printf("创建子线程失败.\n");
            break;
        }
        //设置分离属性
        pthread_detach(tid);
    }
    //关闭套接字
    signal_func(0);
    return 0;
}
