#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/epoll.h>

using namespace std;

const int MAXLINE = 4096;


vector<int>clients;
char buff[4096];
fd_set rset;
int listenfd;
int connfd;
int maxfd = 0;
struct timeval tv;

void readClient()
{
    struct epoll_event ev,events[20];
    int n;
    fcntl(listenfd, F_SETFL, O_NONBLOCK); // 设置非阻塞

    int epfd = epoll_create(10);
    ev.data.fd = listenfd;          //设置与要处理的事件相关的文件描述符
    ev.events = EPOLLIN | EPOLLET;  //设置要处理的事件类型 边缘触发
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    while (1)
    {

        int nfd  = epoll_wait(epfd, events, 20, -1);
        for (int i = 0; i < nfd; ++i) {

            auto fd = events[i].data.fd;
            if(fd == listenfd)
            {
                while ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) > 0) {
                    ev.data.fd = connfd;
                    ev.events = EPOLLIN | EPOLLET;  //设置要处理的事件类型 边缘触发
                    epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
//                    printf("%d login\n", connfd);
                }

            }else{

                if (events[i].events & EPOLLIN)
                {
                    n = recv(fd, buff, MAXLINE, 0);
                    if (n > 0)
                    {
                        buff[n] = '\0';
//                        printf("recv msg from client: %s\n", buff);
                    }else{
//                        printf("del\n");
//                        ev.data.fd = fd;
//                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
//                        printf("del: %s\n", fd);
                        close(fd);

                    }

                }
            }


        }
//        cout << "all event ok " << endl;


    }
}

int main() {
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8888);

    // 设置端口复用
    int on = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    // 创建一个套接字,然后调用 listen 使其
    //       能够自动接收到来的连接并且为连接队列指定一个长度限制.
    if (listen(listenfd, 1024) == -1) {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    printf("======waiting for client's request======\n");



    maxfd = max(listenfd, maxfd);

    thread t(readClient);



    t.join();

    close(listenfd);
    return 0;
}
