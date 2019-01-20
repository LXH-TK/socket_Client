//
//  main.cpp
//  socket_Client
//
//  Created by TK on 2018/12/27.
//  Reference from http://www.linuxhowtos.org/C_C++/socket.htm.
//  Copyright © 2018年 LXH-TK. All rights reserved.
//

/******************************
 * 方法
 * 打开命令行将可执行文件拖入命令行
 * 输入server的hostname
 * 输入server的端口号
 ******************************/

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
// 包含system calls所需的数据种类
#include <sys/types.h>
// 包含socket所需的结构
#include <sys/socket.h>
// 包含internet domain address所需的常量和结构
#include <netinet/in.h>
// 定义结构hostent
#include <netdb.h>
// 多线程
#include <pthread.h>
// 消息队列
#include <sys/ipc.h>
#include <sys/msg.h>
// 监听键盘输入
#include <termios.h>
#include <math.h>

#include "define.hpp"

int msQID;
pthread_t mRecvMsg;
pthread_t mRecvQ;
bool connected = false;

int timeCount = 0;                  // 计算收到的包的数目

struct myMsg
{
    long msgType;
    char msgText[256];
};

/*****************************监听键盘输入*****************************/
static struct termios initial_settings, new_settings;
static int peek_character = -1;
void init_keyboard() {
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag |= ICANON;
    new_settings.c_lflag |= ECHO;
    new_settings.c_lflag |= ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}
void close_keyboard() {
    tcsetattr(0, TCSANOW, &initial_settings);
}
int kbhit() {
    unsigned char ch;
    int nread;
    
    if (peek_character != -1) return 1;
    new_settings.c_cc[VMIN]=0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = (int)read(0, &ch, 1);
    new_settings.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &new_settings);
    if(nread == 1)
    {
        peek_character = ch;
        return 1;
    }
    return 0;
}
int readch() {
    char ch;
    
    if(peek_character != -1)
    {
        ch = peek_character;
        peek_character = -1;
        return ch;
    }
    read(0,&ch,1);
    return ch;
}
/*****************************监听键盘输入*****************************/

void help() {
    printf(" You have connected to the server. Please choose what you want:\n\
          T-GETTIME    N-GETNAME    L-GETLIST\n\
          S-SENDMSG    Q-QUIT\n");
    printf(" Enter: ");
    // 强制刷新
    fflush(stdout);
}

void error(const char *msg) {
    perror(msg);
    exit(0);
}

/*
 * int read(int handle, void *buf, int nbyte);
 * 从handle中读，把内容放到buffer中
 * 返回读写的字节数
 * read和recv区别在报告中
 */
void* myRecvMsg(void *sockfd) {
    
    int j = 0;
    while (connected == true) {
        char buffer[256];
        bzero(buffer, 256);

        // 由于是void*，不知道类型所以先int*，再取值
        int len = (int)recv(*(int*)sockfd, buffer, 255, 0);
        if (len > 0) {
            
            char tempBuffer[256];
            for(int i = 0; i < len; i++){
                if (buffer[i] != DELIMITER) {
                    tempBuffer[j++] = buffer[i];
                }
                else {
                    tempBuffer[j++] = buffer[i];
                    
                    struct myMsg mMsg;
                    mMsg.msgType = 1;
                    memcpy(mMsg.msgText, tempBuffer, CharsWithDeliLen(tempBuffer));
                    msgsnd(msQID, &mMsg, sizeof(struct myMsg), IPC_NOWAIT);
                    timeCount += 1;
                    printf("共收到 %d 个时间\n", timeCount);
                    
                    j = 0;
                }
            }
        }
        
    }
    
    return (void*)0;
}

/*
 * int connect(int sockfd, struct sockaddr *server_addr, int addrlen);
 * 套接字sockfd向地址信息为serv_addr 的socket连接
 * 第一个参数 套接口文件描述符，它是由系统调用socket()返回的
 * 第二个参数 server_addr是指向数据结构sockaddr的指针，其中包括目的端口和IP地址
 * 第三个参数 可以使用sizeof(struct sockaddr)而获得
 * 如果出错则返回-1。成功则返回0
 */
void myConnect(int sockfd, struct sockaddr_in server_addr) {
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error(" ERROR connecting");
    else {
        char buffer[256];
        connected = true;
        bzero(buffer, 256);
        recv(sockfd, buffer, 255, 0);
        printf(" Connect Successfully!\n");
        printf(" %s\n", buffer);
    }
}

/*
 * close(sockfd);
 * 不能再对此套接口做任何的读写操作
 */
void myClose(int sockfd) {
    if (connected == true) {
        close(sockfd);
        connected = false;
    }
    else {
        close(sockfd);
    }
    printf(" Close successfully! Byebye.\n");
}

void getName(int sockfd, Packet TK) {
    printf(" Getting name...\n");
    
    char temp[5] = "NAME";
    TK.req_type = REQ_NAME;
    TK.res_type = RES_REQUEST;
    TK.data = temp;
    TK.length = calcPacketLen(TK);
    
    char *buffer = packetToChars(TK);
    if(send(sockfd, buffer, CharsWithDeliLen(buffer)+1, MSG_WAITALL) < 0) {
        error("ERROR writing to socket(name)");
    }
    help();
}

void getTime(int sockfd, Packet TK) {
    printf(" Getting time...\n");
    
    char temp[5] = "TIME";
    TK.req_type = REQ_TIME;
    TK.res_type = RES_REQUEST;
    TK.data = temp;
    TK.length = calcPacketLen(TK);
    
    for(int i=0; i<100; i++) {
        char *buffer = packetToChars(TK);
        if(send(sockfd, buffer, CharsWithDeliLen(buffer)+1, MSG_WAITALL) < 0) {
            error("ERROR writing to socket(time)");
        }
    }
//    help();
}

void getList(int sockfd, Packet TK) {
    printf(" Getting list...");
    
    char temp[5] = "LIST";
    TK.req_type = REQ_LIST;
    TK.res_type = RES_REQUEST;
    TK.data = temp;
    TK.length = calcPacketLen(TK);
    
    char *buffer = packetToChars(TK);
    if(send(sockfd, buffer, CharsWithDeliLen(buffer)+1, MSG_WAITALL) < 0) {
        error("ERROR writing to socket(list)");
    }
    help();
}

/*
 * int write(int handle, void *buf, int nbyte);
 * 向handle中写buffer中内容
 * 返回读写的字节数
 * send前三个参数相同，报告中有二者区别
 */
void mySend(int sockfd, Packet TK) {
    
    int destination;
    printf(" Please enter destination: ");
    scanf("%d", &destination);
    getchar();
    
    char temp[256];
    printf(" Please enter msg: ");

    // 在没有init_keyboard有问题
    fgets(temp, 255, stdin);
    
    // 使回车符不被包含传输
    int end = (int)strlen(temp);
    if (temp[end-1] == '\n')
        temp[end-1] = 0;
    
    char *data = formMessage(destination, temp);
    TK.req_type = REQ_MSG;
    TK.res_type = RES_REQUEST;
    TK.data = data;
    TK.length = calcPacketLen(TK);
    
    char *buffer = packetToChars(TK);
    if(send(sockfd, buffer, CharsWithDeliLen(buffer)+1, MSG_DONTROUTE) < 0) {
        error("ERROR writing to socket(sending)");
    }
    else {
        help();
    }
}

void* myRecvQ(void *sockfd) {
    
    while (1) {
        int temp_msQId = msQID;
        struct myMsg mMsg;
        if (msgrcv(temp_msQId, &mMsg, sizeof(struct myMsg), 0, IPC_NOWAIT) != -1) {
            
            Packet TK = charsToPacket(mMsg.msgText);
            if (TK.res_type == RES_INSTRUCT && TK.req_type == REQ_MSG) {
                printf(" \n New Msg! %s\n", TK.data);
            }
            else if(TK.req_type == REQ_LIST && TK.res_type == RES_REPLY) {
                printClientList((char*)TK.data);
            }
            else if(TK.req_type == REQ_MSG && TK.res_type == RES_REPLY) {
                int errorCode = getErrorCode(TK.data);
                std::cout << "错误码: " << errorCode << std::endl;
            }
            else {
                printf(" %s\n", TK.data);
            }
            
            help();
        }
    }
    
    return (void*)0;
}

int main(int argc, char *argv[]) {
    
    // 调试需要
    argv[1] = "10.180.169.141";
    argv[2] = "4221";
    
    // 保存用于此socket的值用于之后的操作
    // portNo存储server的端口号
    int sockfd, portNo;
    // 保存server的地址
    struct sockaddr_in server_addr;
    // 指向hostent，定义的是host computer
    struct hostent *server;
    
    Packet TK;
    portNo = atoi(argv[2]);
    
    /*
     * int socket(int domain, int type, int protocol);
     * 第一个参数 AF_INET是IPv4协议，AF_INET6是IPv6协议
     * 第二个参数 是套接口的类型，SOCK_STREAM或SOCK_DGRAM
     * 第三个参数 协议类别，一般为0即可
     * 系统调用socket()返回一个套接口描述符，如果出错，则返回-1
     */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    // 传入server的hostname这一参数(本机为localhost)
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    
    /*
     * void bzero(void *s, int n);
     * 将参数s指定的内存的前n个字节设置为0，通常它用来将套接字地址清0
     */
    bzero((char *) &server_addr, sizeof(server_addr));
    // 协议类型为IPV4
    server_addr.sin_family = AF_INET;
    /*
     * void bcopy(const void *src, void *dst, int n);
     * 从参数src指定的内存区域拷贝n个字节内容到参数dst指定的内存区域。
     */
    bcopy((char *)server->h_addr,
          (char *)&server_addr.sin_addr.s_addr,
          server->h_length);
    server_addr.sin_port = htons(portNo);
    
    // 菜单选项
    printf(" You haven't connected to the server. Please choose what you want:\n\
        C-CONNECT    Q-CLOSE\n Enter: ");
    
    char choice;
    // 判断选项
    choice = getchar();
    getchar();
    if (choice == 'c' || choice == 'C') {
        myConnect(sockfd, server_addr);
    }
    else if(choice == 'q'|| choice == 'Q') {
        myClose(sockfd);
        return 0;
    }
    else {
        printf(" Undefined choice. Please retry.");
        return 0;
    }
    
    time_t t;
    /* 初始化随机数发生器 */
    srand((unsigned) time(&t));
    key_t MSGKEY;
    MSGKEY = rand();
    // 需要保证MSGKEY不同
//    if ( -1 == (MSGKEY = ftok(".", 'a')) ) {
//        perror("Create key error!\n");
//        return 0;
//    }
    std::cout << " 消息队列KEY为：" << MSGKEY << std::endl;
    // msQID = msgget(MSGKEY, IPC_EXCL);
    if ( -1 == (msQID = msgget(MSGKEY, IPC_CREAT | 0777)) ) {
        perror("message queue already exitsted!\n");
        return 0;
    }

    if(msQID < 0) {
        msQID = msgget(MSGKEY,IPC_CREAT | 0666);
        if (msQID < 0) {
            error("ERROR create msg queue");
        }
    }
    // 连接成功创建线程
    if(pthread_create(&mRecvMsg, NULL, myRecvMsg, &sockfd)) {
        printf("子线程创建失败\n");
    }
    
    if (pthread_create(&mRecvQ, NULL, myRecvQ, &sockfd)) {
        printf("子线程创建失败\n");
    }
    
    fflush(stdin);
    fflush(stdout);
    
    // 用这个会阻塞！！！
    init_keyboard();
    // 选择
    help();
    
    while(1){
        
        if (kbhit()) {
            choice = readch();
            getchar();
            
            if (choice == 's' || choice == 'S') {
                mySend(sockfd, TK);
            }
            else if (choice == 't' || choice == 'T') {
                getTime(sockfd, TK);
            }
            else if (choice == 'n' || choice == 'N') {
                getName(sockfd, TK);
            }
            else if (choice == 'l' || choice == 'L') {
                getList(sockfd, TK);
            }
            else {
                myClose(sockfd);
                close_keyboard();
                return 0;
            }
        }
    }
    
    return 0;
}
