#pragma once
#include "peer.h"
//传输数据的最小单位为slice
// 1 slice == 16KB
// piece == 16 * slice == 256KB
typedef struct _Btcache {
    unsigned char *buff;    //缓冲区指针
    int            index;   //当前slice索引
    int            begin;   //当前slice起始位置
    int            length;  //当前slice长度

    unsigned char in_use;  //当前结点是否使用中
    unsigned char read_write;  //是发送的数据还是接收的数据,0表示发送,1表示接收

    unsigned char is_full;    //当前结点是否已满
    unsigned char is_writed;  //当前结点数据是否已经成功写入硬盘
    int           access_count;  //访问次数计数
    struct _Btcache *next;
    // 16KB的缓冲区,保存一个slice
} Btcache;

//根据种子文件信息,预先创建好大小合适(文件大小)的文件
int create_files();

//分配1 slice(16384KB)的内存并初始化
Btcache *initialize_btcache_node();

//创建缓冲区(1024个Btcache)
int create_btcache();

//释放内存,收尾工作
void release_memory_in_btcache();

void release_last_piece();
void clear_btcache();

//从硬盘读取一个slice并存到缓冲区节点中
int read_slice_from_harddisk(Btcache *node);

//从硬盘读取一个piece并存入缓冲区中
int read_piece_from_harddisk(Btcache *p, int index);
int read_last_piece_from_harddisk(Btcache *p, int index);
int read_slice_for_send_last_piece(
    int index, int begin, int length, Peer *peer);
//缓冲区将满,释放一些piece
int release_read_btcache_node(int base_count);
//从缓冲区清除未完成下载的piece
void clear_btcache_before_peer_close(Peer *peer);
//将缓冲区的slice存到peer
int read_slice_for_send(int index, int begin, int length, Peer *peer);

//将peer的slice存到缓冲区
int write_slice_to_btcache(
    int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
//将一个slice存到最后一个piece
int write_slice_to_last_piece(
    int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
//将最后一个piece存到btcache
int write_last_piece_to_btcache(Peer *peer);
//将最后一个piece的最后一个slice存到btcache
int write_last_slice_to_btcache(Peer *peer);
//检查piece正确性并写入
int write_piece_to_harddisk(int sequence, Peer *peer);
//判断写入位置并写入
int write_btcache_node_to_harddisk(Btcache *node);
//缓冲区数据写入硬盘
int write_btcache_to_harddisk(Peer *peer);
// 在peer队列中删除对某个piece的请求
int delete_request_end_mode(int index);
// 下载完一个slice后,检查是否该slice为一个piece最后一块
// 若是则写入硬盘,只对刚刚开始下载时起作用,这样可以立即使peer得知
int is_a_complete_piece(int index, int *sequnce);