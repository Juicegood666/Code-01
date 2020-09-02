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

int sockfd;  //服务器的套接字

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
void *thread_work_func(void *arg);
void signal_work_func(int sig);
char *file_name;  //保存文件名称

//保存文件的信息
#pragma pack(1)
struct FILE_INFO_HEAD
{
    char name[100]; //文件的名称
    unsigned int filesize; //文件大小
};
//定义全局结构体变量
struct FILE_INFO_HEAD file_info_head;

//最大的线程数量
#define MAX_THREAD_CNT 100
//保存线程的信息
struct THREAD_INFO
{
    pthread_t thread_id; //存放所有线程ID
    FILE *fp; //打开的文件
    int fd; //客户端套接字
};

int Thread_GetIndex(struct THREAD_INFO *thread_id);
void Thread_ClearIndex(struct THREAD_INFO *thread_id,pthread_t id);
int Thread_GetThreadID_Index(struct THREAD_INFO *thread,pthread_t id);
int thread_run_flag=1; //线程运行标志
struct THREAD_INFO thread_info[MAX_THREAD_CNT];

int main(int argc,char **argv)
{
    if(argc!=3)
    {
        printf("参数: ./tcp_server <端口号> <将要发送的文件>\n");
        return 0;
    }

    //保存要发送的文件
    file_name=argv[2];

    //得到文件的名称
    strcpy(file_info_head.name,basename(argv[2])); //提取路径中的文件名称
    //得到文件的大小
    struct stat s_buf;
    stat(file_name,&s_buf);
    file_info_head.filesize=s_buf.st_size;
    printf("将要发送的文件名称:%s\n",file_info_head.name);
    printf("发送的文件大小:%d Byte\n",file_info_head.filesize);

    //注册需要捕获的信号
    signal(SIGINT,signal_work_func);
    //忽略 SIGPIPE 信号--方式服务器向无效的套接字写数据导致进程退出
    signal(SIGPIPE,SIG_IGN);

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

    /*4. 等待客户端连接(被动--阻塞)*/
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    int *client_sockfd=NULL; //客户端的套接字
    int index;
    while(1)
    {
        addrlen=sizeof(struct sockaddr_in);
        client_sockfd=malloc(sizeof(int));
        *client_sockfd=accept(sockfd,(struct sockaddr *)&client_addr,&addrlen);
        if(*client_sockfd<0)
        {
            printf("服务器:处理客户端的连接失败.\n");
            break;
        }
        printf("连接上的客户端IP地址:%s\n",inet_ntoa(client_addr.sin_addr));
        printf("连接上的客户端端口:%d\n",ntohs(client_addr.sin_port));
        /*5. 创建子线程与客户端之间实现数据通信*/
        index=Thread_GetIndex(thread_info);//得到空闲的下标
        if(index==-1)
        {
            close(*client_sockfd);
        }
        else
        {
            if(pthread_create(&thread_info[index].thread_id,NULL,thread_work_func,client_sockfd))
            {
                printf("子线程创建失败.\n");
                break;
            }
            //设置分离属性
            pthread_detach(thread_info[index].thread_id);
        }
    }

    /*6. 关闭套接字*/
    signal_work_func(0);
    return 0;
}

//清理线程资源
void clear_resource_thread(void *arg)
{
    struct THREAD_INFO *thread_p=(struct THREAD_INFO*)arg;
    close(thread_p->fd);
    fclose(thread_p->fp);
    printf("%lu 线程资源清理成功.\n",thread_p->thread_id);
}

/*
函数功能: 线程工作函数
*/
void *thread_work_func(void *arg)
{
    int client_fd=*(int*)arg; //取出客户端套接字
    free(arg);

    int select_cnt=0;
    fd_set readfds;
    struct timeval timeout;
    int rx_state;
    unsigned char w_buff[1024];
    int r_cnt;
    int sum_cnt=0;
    int r_val_1=0;
    int r_val_2=0; 
    
    //1. 打开要发送的文件
    FILE *fp=fopen(file_name,"rb");
    if(fp==NULL)
    {
        printf("%s 文件打开失败.\n",file_name);
        close(client_fd);
        pthread_exit(NULL);
    }

    //注册清理函数
    //3种:  pthread_exit 、pthread_cleanup_pop(1); 被其他线程杀死(取消执行)
    int index;
    index=Thread_GetThreadID_Index(thread_info,pthread_self());
    thread_info[index].fp=fp;
    thread_info[index].fd=client_fd;

    pthread_cleanup_push(clear_resource_thread,&thread_info[index]);

    //发送文件头
    write(client_fd,&file_info_head,sizeof(struct FILE_INFO_HEAD));

    //实现与客户端通信
    while(thread_run_flag)
    {
        timeout.tv_sec=0;
        timeout.tv_usec=0;  
        //清空集合
        FD_ZERO(&readfds);
        //添加需要检测的文件描述符
        FD_SET(client_fd,&readfds);
        //检测客户端的IO状态  
        select_cnt=select(client_fd+1,&readfds,NULL,NULL,&timeout);
        if(select_cnt>0)
        {
            //读取客户端发送过来的数据
            r_cnt=read(client_fd,&rx_state,4);
            if(r_cnt<=0)  //判断对方是否断开连接
            {
                printf("服务器提示: 客户端断开连接.\n");
                break;
            }
        }
        else if(select_cnt<0)
        {
            printf("服务器提示: select 函数出现错误.\n");
            break;   
        }

        //读取文件的数据
        r_cnt=fread(w_buff,1,sizeof(w_buff),fp);
        sum_cnt+=r_cnt;  //累加数据

       //计算百分比
        r_val_1=(sum_cnt*1.0/file_info_head.filesize)*100;
        if(r_val_1!=r_val_2)
        {
            r_val_2=r_val_1;
            printf("发送进度:%d %%\n",r_val_2);
        }

        //向客户端发送数据
        if(write(client_fd,w_buff,r_cnt)<0)
        {
            printf("向客户端发送数据失败.\n");
            break;
        }
        if(r_cnt!=sizeof(w_buff))
        {
            printf("文件读取完毕.发送完成.\n");
            break;
        }
    }
    //配套的清理函数
    pthread_cleanup_pop(1);
}

/*
函数功能: 信号处理函数
*/
void signal_work_func(int sig)
{
    thread_run_flag=0; //终止线程的执行
    int i=0;
    printf("正在清理线程资源.\n");
    for(i=0;i<MAX_THREAD_CNT;i++)
    {
        if(thread_info[i].thread_id!=0)
        {
            pthread_cancel(thread_info[i].thread_id);  //必须遇到线程取消点才会结束
            //如何设置取消点?
            //pthread_testcancel(); //判断是否需要结束本身
        }
    }
    sleep(2); //等待线程退出
    printf("服务器正常结束.\n");
    close(sockfd);
    exit(0);
}

/*
函数功能: 获取数组的下标索引
规定: 0表示无效  >0表示有效ID
返回值: -1表示空间满了 正常返回空闲下标值
*/
int Thread_GetIndex(struct THREAD_INFO *thread)
{
    int i=0;
    for(i=0;i<MAX_THREAD_CNT;i++)
    {
        if(thread[i].thread_id==0)return i;
    }
    return -1; 
}

//清除数组里无效的线程的ID
void Thread_ClearIndex(struct THREAD_INFO *thread,pthread_t id)
{
    int i=0;
    for(i=0;i<MAX_THREAD_CNT;i++)
    {
        if(thread[i].thread_id==id)thread[i].thread_id=0;
    }
}

//获取指定线程ID的下标索引
int Thread_GetThreadID_Index(struct THREAD_INFO *thread,pthread_t id)
{
    int i=0;
    for(i=0;i<MAX_THREAD_CNT;i++)
    {
        if(thread[i].thread_id==id)
        {
            return i;
        }
    }
    return -1;
}
