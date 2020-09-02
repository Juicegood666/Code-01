#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
int main(int argc,char **argv)
{
    if(argc!=3)
    {
        printf("正确格式:./a.out <IP> <PORT>");
        return -1;
    }
    int client_sockfd=0;
    client_sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(client_sockfd<0)
    {
        printf("客户端:套接字创建失败.\n");
        return -1;
    }

    struct sockaddr_in client_addr;
    client_addr.sin_family=AF_INET;
    client_addr.sin_port=htons(atoi(argv[2]));
    client_addr.sin_addr.s_addr=inet_addr(argv[1]);
    if(connect(client_sockfd,(const struct sockaddr *)&client_addr,sizeof(struct sockaddr)))
    {
        printf("连接错误\n");
        return -1;
    }
    
    close(client_sockfd);
    return 0;
}