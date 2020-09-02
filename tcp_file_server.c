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
#include <libgen.h>
#define THREAD_MAX_NUM 100

char *file_name=NULL;
int server_sockfd=0;
//存放文件信息结构体
#pragma pack(1)
struct FILE_INFO
{
    char filename[100];
    int file_size;
};
struct FILE_INFO file_info;


//存放子线程信息
struct TID_INFO 
{
    pthread_t tid;
    FILE *fp;
    int fd;
};
struct TID_INFO tid_info[THREAD_MAX_NUM];


/*
函数功能：信号处理函数 
 */
void signal_func(int sig)
{   
    int i=0;
    printf("正在清理线程资源\n");
    for(i=0;i<THREAD_MAX_NUM;i++)
    {
        if(tid_info[i].tid!=0)
        {
            pthread_cancel(tid_info[i].tid);
        }
    }
    sleep(2);
    printf("服务器正常退出.\n");
    close(server_sockfd);
    exit(0);
}


/*
函数功能：获取数组下标索引 
 */
int Thread_GetIndex(struct TID_INFO *tid_info)
{
    int i=0;
    for(i=0;i<THREAD_MAX_NUM;i++)
    {
        if(tid_info[i].tid==0)return i;
    }
    return -1;
}


/*
函数功能：获取指定线程下标索引 
 */
int Thread_GettidIndex(struct TID_INFO *tid_info,pthread_t id)
{
    int i=0;
    for(i=0;i<THREAD_MAX_NUM;i++)
    {
        if(tid_info[i].tid==id)return i;
    }
    return -1;
}


/*
函数功能:线程清理函数 
 */
void clear_resource_thread(void *arg)
{
    struct TID_INFO *resource=(struct TID_INFO *)arg;
    close(resource->fd);
    fclose(resource->fp);
    printf("%lu资源清理成功.\n",resource->tid);
}


//线程工作函数
void *tid_func(void *arg)
{
    int client_sockfd=*(int *)arg;
    free(arg);
    //打开要发送的文件
    FILE *fp=fopen(file_name,"rb");
    if(fp==NULL)
    {
        printf("%s 文件打开失败.\n",file_name);
        close(client_sockfd);
        pthread_exit(NULL);
    }
    int index=Thread_GettidIndex(tid_info,pthread_self());
    tid_info[index].fd=client_sockfd;
    tid_info[index].fp=fp;
    pthread_cleanup_push(clear_resource_thread,&tid_info[index]);
    //将要发送的文件信息发给客户端
    write(client_sockfd,&file_info,sizeof(struct FILE_INFO));
    int select_cnt=0;
    fd_set readfds;
    struct timeval timeout;
    int rx_state=0;
    int rx_cnt=0;
    int num1=0;
    int num2=0;
    int sum_num=0;
    unsigned char wx_buff[1024];
    //与客户端通信
    while(1)
    {
        timeout.tv_sec=0;
        timeout.tv_usec=0;
        //清空事件集合
        FD_ZERO(&readfds);
        //添加要检测的文件描述符
        FD_SET(client_sockfd,&readfds);
        select_cnt=select(client_sockfd+1,&readfds,NULL,NULL,&timeout);
        if(select_cnt>0)
        {
            //读客户端发来的数据
            rx_cnt=read(client_sockfd,&rx_state,4);
            if(rx_cnt<=0)
            {
                printf("客户端断开连接\n");
                break;
            }
        }
        else if(select_cnt<0)
        {
            printf("select函数错误\n");
            break;
        }
        rx_cnt=fread(wx_buff,1,sizeof(wx_buff),fp);
        sum_num+=rx_cnt;
        num1=(sum_num*1.0/file_info.file_size)*100;
        if(num1!=num2)
        {
            num2=num1;
            printf("发送进度:%d %%\n",num2);
        }
        if(write(client_sockfd,wx_buff,rx_cnt)<0)
        {
            printf("发送数据失败\n");
            break;
        }
        if(rx_cnt!=sizeof(wx_buff))
        {
            printf("文件读取完毕.发送完成.\n");
            break;
        }
    }
    close(client_sockfd);
    pthread_cleanup_pop(1);
}



/*
函数功能 
 */
int main(int argc,char **argv)
{
    if(argc!=3)
    {
        printf("正确格式为:./a.out <port> <Send filename>\n");
        return 0;
    }
    file_name=argv[2];
    struct stat sb;
    stat(file_name,&sb);
    strcpy(file_info.filename,basename(argv[2]));
    file_info.file_size=sb.st_size;
    printf("要发送的文件的名字:%s\n",file_info.filename);
    printf("要发送的文件的大小:%d\n",file_info.file_size);
    //注册需要捕获的信号
    signal(SIGINT,signal_func);
    signal(SIGPIPE,SIG_IGN);
    //设置端口号可重用
    int on = 1;
    setsockopt(server_sockfd,SOL_SOCKET,SO_REUSEADDR,&on, sizeof(on));
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
    int index=0;
    while(1)
    {
        //打开日志文件(记录连接的客户端端口号和IP)
        FILE *fp_ipport=fopen("log.txt","a+b");

        client_sockfd=malloc(sizeof(int));
        addrlen=sizeof(struct sockaddr_in);
        *client_sockfd=accept(server_sockfd,(struct sockaddr *)&client_addr,&addrlen);
        if(*client_sockfd<0)
        {
            printf("服务器:客户端连接失败.\n");
            break;
        }
        //写日志
        port_cnt=ntohs(client_addr.sin_port);
        fprintf(fp_ipport,"客户端端口号:%d 客户端IP:%s %s\n",port_cnt,inet_ntoa(client_addr.sin_addr),ctime(&tim));
        fclose(fp_ipport);
        index=Thread_GetIndex(tid_info);
        printf("%d\n",index);
        if(index==-1)
        {
            printf("已达到最大连接个数.\n");
            close(*client_sockfd);
        }
        else
        {
            //创线程
            if(pthread_create(&tid_info[index].tid,NULL,tid_func,client_sockfd))
            {
                printf("创建子线程失败.\n");
                break;
            }
        }
        //设置分离属性
        pthread_detach(tid_info[index].tid);
    }
    //关闭套接字
    signal_func(0);
    return 0;
}