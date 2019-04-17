#pragma once
#include "bitfield.h"
#include <string.h>
#include <time.h>

/*状态定义*/
//已建立TCP连接
#define INITIAL -1
//半握手
#define HALFSHAKED 0
//握手完成
#define HANDSHAKED 1
//发送位图消息
#define SENDBITFIELD 2
//接收位图消息
#define RECVBITFIELD 3
//
#define DATA 4
//超时或任一方将关闭连接
#define CLOSING 5

// 缓冲区的最大大小(2K信息+16Kslice)
#define MSG_SIZE (2 * 1024 + 16 * 1024)

typedef struct _Request_piece {
    //请求piece的索引
    int index;
    //请求piece的偏移
    int begin;
    //请求piece的长度
    int length;

    struct _Request_piece *next;
    //数据请求队列的一个节点,包含请求的piece位置
} Request_piece;
typedef struct _Peer {
    //与peer通信所用的socket
    int socket;
    //该peer的ip地址
    char ip[16];
    //该peer的port
    unsigned short port;
    //该peer的id(20B)
    char id[21];

    //该peer的状态
    int state;
    //是否不给他下载
    int am_choking;
    //是否要从他那里下载
    int am_interested;
    //是否他不让我下载
    int peer_choking;
    //是否他想从我这里下载
    int peer_interested;

    //他的位图
    Bitmap bitmap;

    //他发给我的消息缓冲区
    char *in_buff;
    //长度
    int buff_len;
    //我将给他发的消息缓冲区
    char *out_msg;
    //长度
    int msg_len;
    //我将给他发的消息缓冲区(副本),发送用
    char *out_msg_copy;
    //副本长度
    int msg_copy_len;
    //本次发送数据的末尾位置/下次发送的数据起点位置
    int msg_copy_index;

    //请求数据的队列
    Request_piece *Request_piece_head;
    //被请求数据的队列
    Request_piece *Requested_piece_head;

    //从他下载的字节总数
    unsigned int down_total;
    //给他上传的字节总数
    unsigned int up_total;

    //最新收到的一条消息的时间戳
    time_t start_timestamp, recet_timestamp;
    //最新一次上传下载数据的时间戳
    time_t last_down_timestamp, last_up_timestamp;
    //本周期上传下载字节数,单位Byte
    long long down_count, up_count;
    //本周期上传下载速度,单位Byte/s
    float down_rate, up_rate;

    struct _Peer *next;
    //表示每一个建立连接的peer的结构体
} Peer;

//初始化各个peer成员
int initialize_peer(Peer *peer);
//添加一个peer
Peer *add_peer_node();
//删除一个peer
int del_peer_node(Peer *peer);
//释放(已删除的)peer的内存
void free_peer_node(Peer *peer);
//撤销对peer的请求队列
int cancel_request_list(Peer *peer);
//撤销peer给我的请求队列
int cancel_requested_list(Peer *peer);
//释放(已删除的)peer用到的内存
void release_memory_in_peer();

void print_peers_data();  // for debug