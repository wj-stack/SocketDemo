#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <vector>
#include <thread>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <mutex>
#include <arpa/inet.h>
#include <atomic>
#include <memory>
#include <functional>
#include <queue>
#include <unordered_set>
#include <set>
#include "./ThreadPool/ThreadPool.h"

using namespace std;

class Socket
{
private:
    int sockfd;
    struct sockaddr_in servaddr;
public:
    explicit Socket()  = default;
    ~Socket()
    {
        ::close(sockfd);
    }
    int socket(const char* ip, uint16_t port)
    {

        // 创建stream
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);

        // 创建 ip/port
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htobe16(port);
        ::inet_pton(AF_INET, ip, &servaddr.sin_addr);

        // 设置端口复用
        int on = 1;
        setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );

        // bind
        if (::bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
            printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        // 创建一个套接字,然后调用 listen 使其
        //       能够自动接收到来的连接并且为连接队列指定一个长度限制.
        if (::listen(sockfd, 1024) == -1) {
            printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
            return -1;
        }

        fcntl(sockfd, F_SETFL, O_NONBLOCK); // 设置非阻塞

        return sockfd;
    }

    int fd() const { return sockfd; }

};


class TcpServer
{
private:
    Socket socket;
    int fd;
public:
    using ptr = std::shared_ptr<TcpServer>;
    TcpServer() = default;
    bool init(const char* ip, uint16_t port)
    {
        fd = socket.socket(ip, port);
        return fd > 0;
    }
    int getFd() const {
        return fd;
    }

};

class Event{
private:
public:
    using ptr = std::shared_ptr<Event>;
    void Read(int fd)
    {
        size_t n = 0;
        char buff[10];
        n = recv(fd, buff, 10, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);
        }

    }
    void Write(int fd)
    {
//        cout << fd << "Writing" << endl;
    }
    void Connect(int fd)
    {
//        cout << fd << "login" << endl;
    }
    void Close(int fd)
    {
//        cout << fd << "close" << endl;

    }
};


class SubReactor{
private:
    int epollFd;
    bool isRun{false};
    static const int MaxEvent = 1024;
    vector<epoll_event> events{MaxEvent};
//    struct epoll_event events[MaxEvent]{};
    std::unordered_set<int> clients;
    std::function<void(int)> readCb;
    std::thread thread;
    std::mutex m_mutex;
    int id;
    Event::ptr eventHandel;
    std::threadpool threadpool{10};
public:
    using ptr = std::shared_ptr<SubReactor>;

    explicit SubReactor(int _id, Event::ptr p) : id(_id), eventHandel(std::move(p)) {

        epollFd = epoll_create(EPOLL_CLOEXEC);
//        cout << "creat SubReactor" << endl;
    }
    ~SubReactor()
    {
        ::close(epollFd);
//        cout << "close SubReactor" << endl;

    }
    int getEpollFd() const { return epollFd; }
    int getID() const { return id; }
    int getClientSize()  {
        lock_guard<mutex> lockGuard(m_mutex);
        return clients.size();
    }


    void start()
    {
        thread = std::thread (&SubReactor::run, this);
    }

    void addClient(int fd)
    {
        {
            lock_guard<mutex> lockGuard(m_mutex);
            clients.insert(fd);
        }
        if (!isRun)
        {
            isRun = true;
            start();
        }

    }
    void erase(int fd)
    {
        lock_guard<mutex> lockGuard(m_mutex);
        clients.erase(fd);
    }



    void run()
    {
        const int MAXLINE = 1024;
        while (isRun)
        {
            int nfd  = epoll_wait(epollFd, events.data(), (int)events.size(), -1);
            for (int i = 0; i < nfd; ++i) {
                auto fd = events[i].data.fd;

                if (events[i].events & EPOLLIN) // 有读事件发生
                {
                    threadpool.commit([this, &fd](){
                        eventHandel->Read(fd);
                    });
                }
                if (events[i].events & EPOLLOUT) // 写事件
                {
                    threadpool.commit([this, &fd](){
                        eventHandel->Write(fd);
                    });
                }
                if(events[i].events & EPOLLRDHUP)
                {
                    lock_guard<mutex> lockGuard(m_mutex);
                    clients.erase(fd);
                    ::close(fd);
                    threadpool.commit([this, &fd](){
                        eventHandel->Close(fd);
                    });
                }


            }
            if(nfd >= events.size())
            {
                events.resize(events.size() * 2);
            }

        }
        
    }

};

class MainReactor
{
private:
    int epollFd;
    atomic_bool isRun{true};
    static const int MaxEvent = 10;
    std::vector<epoll_event> events{MaxEvent};
    TcpServer::ptr tcpServer;
    int fd;
    using PIS = std::pair<int,SubReactor::ptr>;
    priority_queue<PIS,vector<PIS>,greater<>> heap;
    std::thread thread;
    mutex _mutex;
    Event::ptr eventHandel;
public:
    explicit MainReactor(TcpServer::ptr p, Event::ptr h, int subReactorSize) : tcpServer(std::move(p)),
                                                                               eventHandel(std::move(h)) {

        epollFd = epoll_create(EPOLL_CLOEXEC);
        fd = tcpServer->getFd();
        add(epollFd, fd);

        for (int i = 0; i < subReactorSize; ++i) {
            auto v = std::make_shared<SubReactor>(i,eventHandel);
            heap.push({0, v});
        }
//        cout << heap.size() << " : heap";
    }
    ~MainReactor()
    {
        ::close(epollFd);
    }

    static void add(int setfd,int listenfd)
    {
        struct epoll_event ev{};
        ev.data.fd = listenfd;          //设置与要处理的事件相关的文件描述符
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;  //设置要处理的事件类型 边缘触发
        epoll_ctl(setfd, EPOLL_CTL_ADD, listenfd, &ev);
    }

    void start()
    {
        thread = std::thread (&MainReactor::run, this);
    }


    void Connect(int connfd)
    {
        {
            lock_guard<mutex> lockGuard{_mutex};
            auto cur = heap.top();
            heap.pop();
            cur.first = cur.second->getClientSize() + 1; // fd+1
            cur.second->addClient(connfd);
            add(cur.second->getEpollFd(), connfd);
            heap.push(cur);
        }
        eventHandel->Connect(connfd);
    }

    void run()
    {
        int connfd;
        struct epoll_event ev{};
        while (isRun)
        {
            int nfd  = epoll_wait(epollFd, events.data(), (int)events.size(), -1);

            for (int i = 0; i < nfd; ++i) {
                if (events[i].data.fd == fd)
                {
                    while ((connfd = accept(fd, (struct sockaddr *) nullptr, nullptr)) > 0) {
//                        threadpool.commit([this, connfd] { Connect(connfd); });
                        Connect(connfd);
                        // 进入事件！
                        // fun();
                    }
                }
            }

            if(nfd >= events.size())
            {
                events.reserve(events.size() * 2);
            }
        }
    }

};


int main() {

    auto tcpServer = make_shared<TcpServer>();
    tcpServer->init("127.0.0.1", 8888);
    MainReactor mainReactor(tcpServer,make_shared<Event>(),10);
    mainReactor.start();

    sleep(1000);
    //129K 并发数最高
    return 0;
}
