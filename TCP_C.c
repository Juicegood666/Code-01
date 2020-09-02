#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
/*
TCP客户端的创建步骤:
1. 创建socket套接字
2. 连接服务器
3. 进行通信
*/
int main(int argc,char **argv)
{
    if(argc!=4)
    {
        printf("正确格式为:./a.out <IP> <port> <filename>\n");
        return 0;
    }
    int client_sockfd=0;
    client_sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(client_sockfd<0)
    {
        printf("客户端创建套接字失败\n");
        return 0;
    }
    struct sockaddr_in client_addr;
    client_addr.sin_family=AF_INET;
    client_addr.sin_port=htons(atoi(argv[2]));
    client_addr.sin_addr.s_addr=inet_addr(argv[1]);
    if(connect(client_sockfd,(const struct sockaddr *)&client_addr,sizeof(struct sockaddr)))
    {
        printf("客户端连接失败\n");
        return 0;
    }
    int cnt=0;
    char buff[1024];
    FILE *fp=fopen(argv[3],"rb");
    if(fp==NULL)
    {
        printf("文件打开失败\n");
        return 0;
    }
    while(1)
    {
        cnt=fread(buff,1,1024,fp);
        printf("客户端发送:%dByte\n",cnt);
        if(write(client_sockfd,buff,cnt)<=0)
        {
            printf("数据发送失败\n");
            break;
        }
        if(cnt!=1024)
        {
            printf("数据发送成功\n");
            break;
        }
    }
    fclose(fp);
    close(client_sockfd);
    return 0;
}
