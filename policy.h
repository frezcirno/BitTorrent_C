#pragma once
#include "peer.h"

//计算各个peer上传下载速度的周期
#define COMPUTE_RATE_TIME 10
//非阻塞peer最大个数
#define UNCHOKE_COUNT 4
//每次请求slice的个数
#define REQ_SLICE_NUM 4

typedef struct _Unchoke_peers {
    //指向非阻塞peer的指针的数组
    Peer *unchkpeer[UNCHOKE_COUNT];
    //非阻塞peer的个数
    int count;
    //优化非阻塞peer
    Peer *optunchkpeer;
    //保存非阻塞及优化peer信息,仅保持一个实例
} Unchoke_peers;

//初始化unchoke_peers链表(本文件中定义)
void init_unchoke_peers();
//选出最快的4个peer并将其unchoke
int select_unchoke_peer();
//选择优化非阻塞peer
int select_optunchoke_peer();
//计算一个周期内每个peer的上传下载速度
int compute_rate();
//计算总的上传下载速度
int compute_total_rate();

//判断某个peer是不是种子
int is_seed(Peer *node);
//构造数据请求
int create_req_slice_msg(Peer *node);
