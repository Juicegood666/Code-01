#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
/* According to POSIX.1-2001 */
#include <sys/select.h>
/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/epoll.h>

int sockfd;  //服务器的套接字

//最大监听的数量
#define MAX_EVENTS 100
struct epoll_event events[MAX_EVENTS];
int epfd;

void signal_work_func(int sig);

//定义结构体,保存连接上服务器的所有客户端
#pragma pack(1)
struct Client_FD
{
    int fd;
    struct Client_FD *next;
};
struct Client_FD *list_head=NULL; //定义链表头
struct Client_FD * LIST_HeadInit(struct Client_FD *list_head);
void List_AddNode(struct Client_FD *list_head,int fd);
void ListDelNode(struct Client_FD *list_head,int fd);

//结构体: 消息结构体
struct SEND_DATA
{
    char name[100]; //昵称
    char data[100]; //发送的实际聊天数据
    char stat;      //状态: 0x1 上线  0x2 下线  0x3 正常数据
};
//转发消息
void Client_SendData(int client_fd,struct Client_FD *list_head,struct SEND_DATA *sendata);

int main(int argc,char **argv)
{
    if(argc!=2)
    {
        printf("参数: ./tcp_server <端口号>\n");
        return 0;
    }
    //注册需要捕获的信号
    signal(SIGINT,signal_work_func);
    //忽略 SIGPIPE 信号--方式服务器向无效的套接字写数据导致进程退出
    signal(SIGPIPE,SIG_IGN);

    //初始化链表头
    list_head=LIST_HeadInit(list_head);
    
    /*1. 创建socket套接字*/
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0)
    {
        printf("服务器:套接字创建失败.\n");
        return 0;
    }

    int on = 1;
    //设置端口号可以复用
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /*2. 绑定端口号*/
    struct sockaddr_in addr;
    addr.sin_family=AF_INET; //IPV4
    addr.sin_port=htons(atoi(argv[1])); //65535
    //addr.sin_addr.s_addr=inet_addr("192.168.2.16");
    addr.sin_addr.s_addr=INADDR_ANY; //本地所有IP地址 "0.0.0.0"
    if(bind(sockfd,(const struct sockaddr *)&addr,sizeof(struct sockaddr)))
    {
        printf("服务器:端口号绑定失败.\n");
        return 0;
    }
    /*3. 设置等待连接的客户端数量*/
    if(listen(sockfd,100))
    {
        printf("设置等待连接的客户端数量识别.\n");
        return 0;
    }

    /*4. 创建epoll函数专用文件描述符*/
    epfd=epoll_create(MAX_EVENTS);
    if(epfd<0)
    {
        printf(" 创建epoll函数专用文件描述符失败.\n");
        return 0;
    }

    /*5. 添加监听事件*/
    struct epoll_event event;
    event.events=EPOLLIN; //可读事件
    event.data.fd=sockfd; //要操作的文件描述符
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&event);

    //定义结构体变量--客户端发送过来的数据
    struct SEND_DATA sendata;
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    int client_sockfd=0; //客户端的套接字
    int epoll_stat;
    int i;
    int r_cnt;
    while(1)
    {
        epoll_stat=epoll_wait(epfd,events,MAX_EVENTS,-1);
        if(epoll_stat>0)
        {
            for(i=0;i<epoll_stat;i++)
            {
                //表示是服务器收到新的客户端连接请求
                if(events[i].data.fd==sockfd)
                {
                    addrlen=sizeof(struct sockaddr_in);
                    client_sockfd=accept(sockfd,(struct sockaddr *)&client_addr,&addrlen);
                    if(client_sockfd<0)
                    {
                        printf("服务器:处理客户端的连接失败.\n");
                    }
                    else
                    {
                        //保存客户端套接字描述符
                        List_AddNode(list_head,client_sockfd);
                        //添加要监听的新客户端
                        event.events=EPOLLIN; //可读事件
                        event.data.fd=client_sockfd; //要操作的文件描述符
                        epoll_ctl(epfd,EPOLL_CTL_ADD,client_sockfd,&event);
                        printf("连接上的客户端IP地址:%s\n",inet_ntoa(client_addr.sin_addr));
                        printf("连接上的客户端端口:%d\n",ntohs(client_addr.sin_port));
                    }
                }
                //表示客户端有新的数据发送给服务器
                else
                {
                     //读取客户端发送过来的数据
                    r_cnt=read(events[i].data.fd,&sendata,sizeof(struct SEND_DATA));
                    if(r_cnt<=0)  //判断对方是否断开连接
                    {
                        sendata.stat=0x2; //下线
                        Client_SendData(events[i].data.fd,list_head,&sendata); //转发下线消息
                        ListDelNode(list_head,events[i].data.fd); //删除当前的套接字
                        printf("服务器提示: 客户端断开连接.\n");

                        //卸载已经无效的客户端套接字描述符
                        event.events=EPOLLIN; //可读事件
                        event.data.fd=events[i].data.fd; //要操作的文件描述符
                        epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&event);
                        continue;
                    }
                    Client_SendData(events[i].data.fd,list_head,&sendata);
                }   
            }  
        }
        else if(epoll_stat<0)
        {
            printf("epoll_wait出现错误.\n");
            break;
        }
    }
    /*6. 关闭套接字*/
    signal_work_func(0);
    return 0;
}

/*
函数功能: 信号处理函数
*/
void signal_work_func(int sig)
{
    printf("正在清理线程资源.\n");
    struct Client_FD *p=list_head;
    while(p->next)
    {
        p=p->next;
        close(p->fd);
    }
    printf("服务器正常结束.\n");
    close(epfd);
    close(sockfd);
    exit(0);
}

//初始化链表头
struct Client_FD * LIST_HeadInit(struct Client_FD *list_head)
{
    if(list_head==NULL)
    {
        list_head=malloc(sizeof(struct Client_FD));
        list_head->next=NULL;
    }
    return list_head;
}

//添加节点
void List_AddNode(struct Client_FD *list_head,int fd)
{
    struct Client_FD *p=list_head;
    struct Client_FD *new_p;
    while(p->next)
    {
        p=p->next;
    }
    new_p=malloc(sizeof(struct Client_FD));
    new_p->next=NULL;
    new_p->fd=fd;
    p->next=new_p;
}

//删除节点
void ListDelNode(struct Client_FD *list_head,int fd)
{
    struct Client_FD *p=list_head;
    struct Client_FD *tmp;
    while(p->next)
    {
        tmp=p;
        p=p->next;
        if(p->fd==fd)
        {
            tmp->next=p->next;
            free(p);
            break;
        }
    }
}

/*
函数功能: 向在线的所有客户端发送消息
状态: 0x1 上线  0x2 下线  0x3 正常数据
*/
void Client_SendData(int client_fd,struct Client_FD *list_head,struct SEND_DATA *sendata)
{
    struct Client_FD *p=list_head;
    while(p->next)
    {
        p=p->next;
        if(p->fd!=client_fd)
        {
            write(p->fd,sendata,sizeof(struct SEND_DATA));
        } 
    }
}
