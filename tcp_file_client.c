#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
char *name;
char path[100];
#define PATH "/mnt/hgfs/ubuntu_share_file/file/"
#pragma pack(1)
struct FILE_INFO
{
    char filename[100];
    int file_size;
};
struct FILE_INFO file_info;

int main(int argc,char **argv)
{
    if(argc!=3)
    {
        printf("正确格式:./a.out <IP> <PORT>\n");
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
    

    //读取文件信息
    if(read(client_sockfd,&file_info,sizeof(struct FILE_INFO))!=sizeof(struct FILE_INFO))
    {
        printf("读取文件失败\n");
        return 0;
    }
    printf("要接收的文件的名字:%s\n",file_info.filename);
    printf("要接收的文件的大小:%d\n",file_info.file_size); 
    name=file_info.filename;
    strcpy(path,PATH);
    strcat(path,name);
    FILE *recv_fp=fopen(path,"wb");
    if(recv_fp==NULL)
    {
        printf("文件创建失败\n");
        return 0;
    }
    fd_set readfds;
    struct timeval timeout;
    int select_cnt=0;
    int rx_cnt=0;
    int sum_num=0;
    int num1=0;
    int num2=0;
    unsigned char buff[1024];
    while(1)
    {
         //清空集合
        FD_ZERO(&readfds);
        //添加需要检测的文件描述符
        FD_SET(client_sockfd,&readfds);
        //检测客户端的IO状态  
        select_cnt=select(client_sockfd+1,&readfds,NULL,NULL,NULL);
        if(select_cnt>0)
        {
            rx_cnt=read(client_sockfd,buff,sizeof(buff));
            if(rx_cnt<=0)
            {
                printf("服务器断开连接.\n");
                break;
            }
            
            sum_num+=rx_cnt;
            num1=(sum_num*1.0/file_info.file_size)*100;
            if(num1!=num2)
            {
                num2=num1;
                printf("发送进度:%d %%\n",num2);
            }
            fwrite(buff,1,rx_cnt,recv_fp);

            if(sum_num==file_info.file_size)
            {
                printf("文件接收完毕.\n");
                break;
            }
        }
        if(select_cnt<0)
        {
            printf("select函数错误.\n");
            break;
        }
    }
    close(client_sockfd);
    fclose(recv_fp);
    return 0;
}