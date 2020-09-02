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
void *tid_func(void *arg)
{

}
int main(int argc,char **argv)
{
    if(argc!=3)
    {
        printf("正确格式为:./a.out <port> <filename>\n");
        return 0;
    }
    int server_sockfd=0;
    int client_sockfd=0;
    //创建socket套接字(文件描述符)
    server_sockfd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr=INADDR_ANY;
    //server_addr.sin_addr.s_addr=inet_addr("192.168.43.1");
    //绑定端口号(创建服务器:提供端口号和IP地址)
    if(bind(server_sockfd,(const struct sockaddr *)&server_addr,sizeof(struct sockaddr)))
    {
        printf("创建端口号失败\n");
        return 0;
    }
    //设置监听的客户端数量(设置待处理的队列最大缓存待连接的客户端数量)
    if(listen(server_sockfd,100))
    {
       printf("设置等待的客户端数量失败\n");
        return 0;
    }
    FILE *fp_ipport=fopen("log.txt","a+b");
    uint16_t port_cnt=0;
    socklen_t addrlen=sizeof(struct sockaddr);
    client_sockfd=accept(server_sockfd,(struct sockaddr *)&client_addr,&addrlen);
    if(client_sockfd<0)
    {
        printf("服务器:客户端连接失败.\n");
        return 0;
    }
    printf("连接服务器的客户端的IP:%s\n",inet_ntoa(client_addr.sin_addr));
    printf("连接服务器的客户端的端口号:%d\n",ntohs(client_addr.sin_port));
    port_cnt=ntohs(client_addr.sin_port);
    time_t tim=time(NULL);
    fprintf(fp_ipport,"客户端端口号:%d 客户端IP:%s %s\n",port_cnt,inet_ntoa(client_addr.sin_addr),ctime(&tim));
    fclose(fp_ipport);
    char buff[1024];
    int cnt;
    FILE *fp=fopen(argv[2],"wb");
    if(fp==NULL)
    {
        printf("文件创建失败.\n");
        return 0;
    }
    while(1)
    {
        cnt=read(client_sockfd,buff,1024);
        if(cnt<=0)
        {
            printf("读取失败.\n");
            break;
        }
        fwrite(buff,1,cnt,fp);
        if(cnt!=1024)
        {
            printf("数据读取完毕.\n");
            break;
        }
    }
    fclose(fp);
    close(client_sockfd);
}