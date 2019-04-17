/**
 * 构造HTTP请求,从tracker服务器获取peer信息
 * **/

#pragma once
//解析返回数据
#include "parse_metafile.h"
#include <netinet/in.h>

typedef struct _Peer_addr {
    char               ip[16];
    unsigned short     port;
    struct _Peer_addr *next;
    // peer的地址(ip和port)
} Peer_addr;

// HTTP编码(把字母数字以外的字符转换成百分号编码)
int http_encode(unsigned char *in, int len1, char *out, int len2);
//从种子文件中获取tracker主机名
int get_tracker_name(Announce_list *node, char *name, int len);
//从种子文件中获取tracker端口号
int get_tracker_port(Announce_list *node, unsigned short *port);

//构造HTTP GET请求
int create_request(char *request,
    int                  len,
    Announce_list *      node,
    unsigned short       port,
    long long            down,
    long long            up,
    long long            left,
    int                  numwant);

//非阻塞方式连接tracker,更新最大文件描述符的值
int prepare_connect_tracker(int *max_sockfd);
//非阻塞方式连接peer,更新最大文件描述符的值
int prepare_connect_peer(int *max_sockfd);

//获取tracker返回消息类型, total_length存放tracker返回数据的长度
int get_response_type(char *buffer, int len, int *total_length);
//解析tracker返回消息类型1
int parse_tracker_response1(char *buffer, int ret, char *redirection, int len);
//解析tracker返回消息类型2
int parse_tracker_response2(char *buffer, int ret);
//将已建立连接的peer加到链表中
int add_peer_node_to_peerlist(int *sock, struct sockaddr_in saptr);
//释放peer_addr链表,善后工作
void free_peer_addr_head();
