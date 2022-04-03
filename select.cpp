#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <fcntl.h>

using namespace std;

const int MAXLINE = 4096;


vector<int>clients;
char buff[4096];
fd_set rset;
int listenfd;
int connfd;
int maxfd = 0;
struct timeval tv;

//int select(int nfds, fd_set *restrict readfds,
//           fd_set *restrict writefds, fd_set *restrict exceptfds,
//           struct timeval *restrict timeout);
//
//void FD_CLR(int fd, fd_set *set);
//int  FD_ISSET(int fd, fd_set *set);
//void FD_SET(int fd, fd_set *set);
//void FD_ZERO(fd_set *set);


// bitmap 1024
// set 不可重用
// 用户态内核态来回拷贝
// O(n) 再次遍历
void readClient()
{

    int n;
    fcntl(listenfd, F_SETFL, O_NONBLOCK); // 设置非阻塞
    while (1)
    {
        FD_ZERO(&rset);
        FD_SET(listenfd,&rset);
        maxfd = listenfd;
        for(auto& client:clients)
        {
            FD_SET(client, &rset); // 重新加入到set集合中
            maxfd = max(maxfd, client);
        }
        int res = select(maxfd + 1, &rset, nullptr, nullptr, nullptr);

        if (res < 0) {
            perror("select");
            break;
        } else if (res == 0) {
            perror("continue");
            continue;
        }

        if(FD_ISSET(listenfd,&rset)) // 有新客户端链接
        {
            if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1) {
                printf("accept socket error: %s(errno: %d)", strerror(errno), errno);
                continue;
            }
            clients.push_back(connfd);
            fcntl(connfd, F_SETFL, O_NONBLOCK); // 设置非阻塞

//            cout << connfd << "login" << "all client:" << clients.size() << endl;
//            for (const auto &item: clients) {
//                cout << item << ' ';
//            }
//            cout << endl;
        }

        for (auto client = clients.begin();client != clients.end() ; ) {
            if (!FD_ISSET(*client, &rset))
            {
                client++;
                continue;
            }
            n = recv(*client, buff, MAXLINE, 0);
            if (n > 0)
            {
                buff[n] = '\0';
                printf("recv msg from client: %s\n", buff);
                client++;
            }else{
                close(*client);
                client = clients.erase(client);
            }
        }



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
