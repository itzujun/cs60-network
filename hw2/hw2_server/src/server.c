#include <netinet/in.h>    // for sockaddr_in
#include <sys/types.h>    // for socket
#include <sys/socket.h>    // for socket
#include <stdio.h>        // for printf
#include <stdlib.h>        // for exit
#include <string.h>        // for bzero
/*
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
*/
#define HELLO_WORLD_SERVER_PORT    47789
#define LENGTH_OF_LISTEN_QUEUE 20
#define BUFFER_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

int main(int argc, char **argv)
{
    //设置一个socket地址结构server_addr,代表服务器internet地址, 端口
    struct sockaddr_in server_addr;
    bzero(&server_addr,sizeof(server_addr)); //把一段内存区的内容全部设置为0
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(HELLO_WORLD_SERVER_PORT);

    //创建用于internet的流协议(TCP)socket,用server_socket代表服务器socket
    int server_socket = socket(PF_INET,SOCK_STREAM,0);
    if( server_socket < 0)
    {
        printf("Create Socket Failed!");
        exit(1);
    }
{
   int opt =1;
   setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
}

    //把socket和socket地址结构联系起来
    if( bind(server_socket,(struct sockaddr*)&server_addr,sizeof(server_addr)))
    {
        printf("Server Bind Port : %d Failed!", HELLO_WORLD_SERVER_PORT);
        exit(1);
    }

    //server_socket用于监听
    if ( listen(server_socket, LENGTH_OF_LISTEN_QUEUE) )
    {
        printf("Server Listen Failed!");
        exit(1);
    }
    while (1) //服务器端要一直运行
    {
        //定义客户端的socket地址结构client_addr
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);

        //接受一个到server_socket代表的socket的一个连接
        //如果没有连接请求,就等待到有连接请求--这是accept函数的特性
        //accept函数返回一个新的socket,这个socket(new_server_socket)用于同连接到的客户的通信
        //new_server_socket代表了服务器和客户端之间的一个通信通道
        //accept函数把连接到的客户端信息填写到客户端的socket地址结构client_addr中
        int new_server_socket = accept(server_socket,(struct sockaddr*)&client_addr,&length);
        if ( new_server_socket < 0)
        {
            printf("Server Accept Failed!\n");
            break;
        }

        char buffer[BUFFER_SIZE];
        bzero(buffer, BUFFER_SIZE);
        length = recv(new_server_socket,buffer,BUFFER_SIZE,0);
        if (length < 0)
        {
            printf("Server Recieve Data Failed!\n");
            break;
        }
        puts(buffer);

                //发送buffer中的字符串到new_server_socket,实际是给客户端
                if(send(new_server_socket,buffer,strlen(buffer),0)<0)
                {
                    printf("Responding failed! \n");
                    break;
                }
                bzero(buffer, BUFFER_SIZE);

        //关闭与客户端的连接
        close(new_server_socket);
    }
    //关闭监听用的socket
    close(server_socket);
    return 0;
}
