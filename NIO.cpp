#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <set>
#include <mutex>

using namespace std;

const int MAXLINE = 4096;
std::mutex m_mutex;

// BIO

set<int>clients;
char buff[4096];

//3.3 非阻塞 IO 的优点
//正如前面提到的，非阻塞 IO 解决了阻塞 IO每个连接一个线程处理的问题，所以其最大的优点就是 一个线程可以处理多个连接，这也是其非阻塞决定的。
//3.4 非阻塞 IO 的缺点
//
//        但这种模式，也有一个问题，就是需要用户多次发起系统调用。频繁的系统调用是比较消耗系统资源的。
//
//因此，既然存在这样的问题，那么自然而然我们就需要解决该问题：保留非阻塞 IO 的优点的前提下，减少系统调用

//多路复用主要复用的是通过有限次的系统调用来实现管理多个网络连接。
//最简单来说，我目前有 10 个连接，
//我可以通过一次系统调用将这 10 个连接都丢给内核，
//让内核告诉我，
//哪些连接上面数据准备好了，然后我再去读取每个就绪的连接上的数据。
//因此，IO 多路复用，复用的是系统调用。
//通过有限次系统调用判断海量连接是否数据准备好了

void readAllClient()
{
    while (1) // 虽然不阻塞，但是系统调用一直要调用
    {
        int n;
        for (auto client = clients.begin(); client != clients.end(); ++client) {
            n = recv(*client, buff, MAXLINE, 0);
            if (n > 0)
            {
                buff[n] = '\0';
                printf("recv msg from client: %s\n", buff);
            }else{
                lock_guard<mutex> lockGuard(m_mutex);
                clients.erase(client);
            }
        }

    }
}

int main() {
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8888);

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    // 创建一个套接字,然后调用 listen 使其
    //       能够自动接收到来的连接并且为连接队列指定一个长度限制.
    if (listen(listenfd, 10) == -1) {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    printf("======waiting for client's request======\n");

    // 防止端口重用
    int on = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

    thread t(readAllClient);

    while (1) {
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1) {
            printf("accept socket error: %s(errno: %d)", strerror(errno), errno);
            continue;
        }
        fcntl(connfd, F_SETFL, O_NONBLOCK); // 设置非阻塞

        {
            lock_guard<mutex> lockGuard(m_mutex);
            clients.insert(connfd);
        }//        cout << connfd << "login" << endl;
//        close(connfd);
    }
    close(listenfd);
    return 0;
}
