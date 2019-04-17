#include "policy.h"
#include "data.h"
#include "message.h"
#include "parse_metafile.h"
#include "peer.h"
#include "policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//总的下载上传量
long long total_down = 0L, total_up = 0L;
//总的下载上传速度
float total_down_rate = 0.0F, total_up_rate = 0.0F;
// 已连接的peer个数
int total_peers = 0;

// peer链表
extern Peer *peer_head;
//存储未阻塞的peers,全局唯一
Unchoke_peers unchoke_peers;

//是否处于下载最后阶段
extern int end_mode;
//己方的位图
extern Bitmap *bitmap;
//所有piece的hash值的总长度(20*n)
extern int pieces_hash_length;
//每个piece长度
extern int piece_length;

extern Btcache *btcache_head;
extern int      last_piece_index;
extern int      last_piece_count;
extern int      last_slice_len;
extern int      download_piece_num;
//打乱piece列表用
int *rand_num = NULL;

void init_unchoke_peers()
{
    int i;

    //各项全置为0
    for (i = 0; i < UNCHOKE_COUNT; i++) *(unchoke_peers.unchkpeer + i) = NULL;
    unchoke_peers.count        = 0;
    unchoke_peers.optunchkpeer = NULL;
}

//不考虑optunchkpeer
int is_in_unchoke_peers(Peer *node)
{
    int i;

    if (node == NULL) return -1;
    for (i = 0; i < unchoke_peers.count; i++)
        if (unchoke_peers.unchkpeer[i] == node) return 1;
    // if (unchoke_peers.optunchkpeer == node) return 1;
    return 0;
}

//从unchoke_peers中获取最慢的一个peer的索引
int get_last_index(Peer **array, int len)
{
    int i, j = -1;

    if (len <= 0)
        return j;
    else
        j = 0;

    for (i = 0; i < len; i++)
        if (array[i]->down_rate < array[j]->down_rate) j = i;

    return j;
}

int select_unchoke_peer()
{
    Peer *p;
    Peer *now_fast[UNCHOKE_COUNT];  //记录很快的peer
    Peer *now_slow[UNCHOKE_COUNT];
    int   i, j, index = 0, len = UNCHOKE_COUNT;

    //记录本轮选择出的需要unchoke和choke的socket
    int unchoke_socket[UNCHOKE_COUNT], choke_socket[UNCHOKE_COUNT];

    //初始化
    for (i = 0; i < len; i++) {
        now_fast[i]       = NULL;
        now_slow[i]       = NULL;
        unchoke_socket[i] = -1;
        choke_socket[i]   = -1;
    }

    //将unchoke_peers中的失效peer清除
    for (i = 0, j = 0; i < unchoke_peers.count; i++) {
        //遍历链表找处于unchoke队列中的peer
        p = peer_head;
        while (p != NULL) {
            if (p == unchoke_peers.unchkpeer[i]) break;
            p = p->next;
        }
        //没找到(该peer已断开连接)就去除该peer,清除的个数为j
        if (p == NULL) {
            unchoke_peers.unchkpeer[i] = NULL;
            j++;
        }
    }
    if (j != 0) {
        unchoke_peers.count -= j;
        //搜索unchkpeer数组,将有效项临时保存到force_choke中
        for (i = 0, j = 0; i < len; i++) {
            if (unchoke_peers.unchkpeer[i] != NULL) {
                now_slow[j] = unchoke_peers.unchkpeer[i];
                j++;
            }
        }
        //再放回unchoke_peers
        for (i = 0; i < len; i++) {
            unchoke_peers.unchkpeer[i] = now_slow[i];
            now_slow[i]                = NULL;
        }
    }

    //将unchoke_peer中变慢的peer加入到now_slow中
    for (i = 0, j = -1; i < unchoke_peers.count; i++)
        if ((unchoke_peers.unchkpeer)[i]->up_rate > 50 * 1024 &&
            (unchoke_peers.unchkpeer)[i]->down_rate < 0.1 * 1024) {
            //上传>50Kb/s而下载<0.1Kb/s
            j++;
            now_slow[j] = unchoke_peers.unchkpeer[i];
        }
    //将符合条件较快的peer加入到now_fast中
    // index记录最快peer的总数
    p = peer_head;
    while (p != NULL) {
        //寻找 (已建立连接) 而且 (我有他没有的piece) 而且 (不是种子) 而且
        //(不是很慢) 的peer
        if (p->state == DATA && is_interested(bitmap, &(p->bitmap)) &&
            is_seed(p) != 1) {
            //如果该peer被force_choke(很慢),忽略之
            for (i = 0; i < len; i++)
                if (p == now_slow[i]) break;
            //没有被choke
            if (i == len) {
                if (index < UNCHOKE_COUNT) {
                    //未满
                    now_fast[index] = p;
                    index++;
                } else {
                    //已满,替换最慢的peer
                    j = get_last_index(now_fast, UNCHOKE_COUNT);
                    if (p->down_rate >= now_fast[j]->down_rate) now_fast[j] = p;
                }
            }
        }
        p = p->next;
    }

    //重置全局的unchoke_peer,将其放在choke列表中
    for (i = 0; i < unchoke_peers.count; i++)
        choke_socket[i] = (unchoke_peers.unchkpeer)[i]->socket;

    //将最快的peer的socket加入到unchoke_socket中
    //如果最快的peer上次已经是unchoke的,则置-1标记,不再更改
    for (i = 0; i < index; i++) {
        if (is_in_unchoke_peers(now_fast[i]) == 1) {
            unchoke_socket[i] = -1;
            choke_socket[i]   = -1;
        } else
            unchoke_socket[i] = now_fast[i]->socket;
    }

    /*已找到需要choke和unchoke的peers的socket,以及最快的peer列表(共index个)*/

    //将最快的peer都加入全局unchkpeer
    for (i = 0; i < index; i++) (unchoke_peers.unchkpeer)[i] = now_fast[i];
    unchoke_peers.count = index;

    // 状态变化后,要对各个peer的状态值重新赋值,并且创建choke、unchoke消息
    p = peer_head;
    while (p != NULL) {
        for (i = 0; i < len; i++) {
            if (unchoke_socket[i] == p->socket && unchoke_socket[i] != -1) {
                p->am_choking = 0;
                create_chock_interested_msg(1, p);
            }
            if (choke_socket[i] == p->socket && unchoke_socket[i] != -1) {
                p->am_choking = 1;
                cancel_request_list(p);
                create_chock_interested_msg(0, p);
            }
        }
        p = p->next;
    }
#ifdef DEBUG
    printf("Unchk peer is: ");
    for (int t = 0; t < unchoke_peers.count; t++)
        printf("Unchk peer %d: %s\n", t, unchoke_peers.unchkpeer[t]->id);
#endif
    return 0;
}

//生成[0,piece_count)打乱后的数组
int get_rand_numbers(int length)
{
    int i, index, t;

    rand_num = (int *)malloc(length * sizeof(int));
    if (length == 0 || rand_num == NULL) return -1;
    srand(time(NULL));
    for (i = 0; i < length; i++) rand_num[i] = i;
    for (i = 0; i < length; i++) {
        index           = (rand() % (length - i) + i);
        t               = rand_num[index];
        rand_num[index] = rand_num[i];
        rand_num[i]     = t;
    }

    return 0;
}

//选择优化非阻塞peer
int select_optunchoke_peer()
{
    // peer总数
    int   count = 0;
    int   index, i, j, ret;
    Peer *p = peer_head;

    // 获取peer队列中peer的总数
    while (p != NULL) {
        count++;
        p = p->next;
    }

    // 如果peer总数太少(小于等于4),则没有必要选择优化非阻塞peer
    if (count <= UNCHOKE_COUNT) return 0;

    ret = get_rand_numbers(count);
    if (ret < 0) {
        printf("%s:%d get_bit_value rand numbers error\n", __FILE__, __LINE__);
        return -1;
    }
    i = 0;
    while (i < count) {
        // 随机选择一个数,该数在0～count-1之间
        index = rand_num[i];
        // 找到索引为index的这个peer
        p = peer_head;
        j = 0;
        while (j < index && p != NULL) {
            p = p->next;
            j++;
        }
        //如果该peer(已连接)而且(正在被choke)而且(不是优化非阻塞peer)而且(非种子)而且(我有他没有的piece)
        if (p->state == DATA && is_in_unchoke_peers(p) != 1 &&
            p != unchoke_peers.optunchkpeer && is_seed(p) != 1 &&
            is_interested(bitmap, &(p->bitmap))) {
            //已经有优化非阻塞peer了
            if ((unchoke_peers.optunchkpeer) != NULL) {
                //找到原来的优化非阻塞peer
                Peer *temp = peer_head;
                while (temp != NULL) {
                    if (temp == unchoke_peers.optunchkpeer) break;
                    temp = temp->next;
                }
                //如果这个peer还存在,就阻塞这个peer
                if (temp != NULL) {
                    (unchoke_peers.optunchkpeer)->am_choking = 1;
                    create_chock_interested_msg(0, unchoke_peers.optunchkpeer);
                }
            }
            //解除这个天选之子的阻塞,并告诉他
            p->am_choking = 0;
            create_chock_interested_msg(1, p);
            unchoke_peers.optunchkpeer = p;
            printf("*** optunchoke:%s ***\n", p->ip);
            break;  //找到就结束循环,否则继续
        }
        i++;
    }

    if (rand_num != NULL) {
        free(rand_num);
        rand_num = NULL;
    }
    return 0;
}
int compute_rate()
{
    Peer * p        = peer_head;
    time_t time_now = time(NULL);
    long   t        = 0;

    while (p != NULL) {
        //计算下载速度
        if (p->last_down_timestamp == 0) {
            p->down_rate  = 0.0f;
            p->down_count = 0;
        } else {
            //时间差
            t = time_now - p->last_down_timestamp;
            if (t == 0)
                printf("%s:%d time is 0\n", __FILE__, __LINE__);
            else
                p->down_rate = p->down_count / t;
            //重置下载量,时间戳
            p->down_count          = 0;
            p->last_down_timestamp = 0;
        }
        //计算上传速度
        if (p->last_up_timestamp == 0) {
            p->up_rate  = 0.0f;
            p->up_count = 0;
        } else {
            t = time_now - p->last_up_timestamp;
            if (t == 0)
                printf("%s:%d time is 0\n", __FILE__, __LINE__);
            else
                p->up_rate = p->up_count / t;
            p->up_count          = 0;
            p->last_up_timestamp = 0;
        }

        p = p->next;
    }  // while
#ifdef DEBUG
    for (p = peer_head; p != NULL; p = p->next)
        printf("@Peer: %s Up:%d Down:%d ", p->id, p->up_rate, p->down_rate);
    printf("\n");
#endif

    return 0;
}
int compute_total_rate()
{
    Peer *p = peer_head;

    total_peers     = 0;
    total_down      = 0;
    total_up        = 0;
    total_down_rate = 0.0f;
    total_up_rate   = 0.0f;

    while (p != NULL) {
        total_down += p->down_total;
        total_up += p->up_total;
        total_down_rate += p->down_rate;
        total_up_rate += p->up_rate;

        total_peers++;
        p = p->next;
    }
    return 0;
}

int is_seed(Peer *node)
{
    int           i;
    unsigned char c       = (unsigned char)0xFF, last_byte;
    unsigned char cnst[8] = {255, 254, 252, 248, 240, 224, 192, 128};

    if (node->bitmap.bitfield == NULL) return 0;

    for (i = 0; i < node->bitmap.bitfield_length - 1; i++)
        if ((node->bitmap.bitfield)[i] != c) return 0;

    // 判断最后一个字节
    last_byte = node->bitmap.bitfield[i];
    // 获取最后一个字节的无效位数
    i = 8 * node->bitmap.bitfield_length - node->bitmap.valid_length;
    if (last_byte >= cnst[i])
        return 1;
    else
        return 0;
}
int create_req_slice_msg(Peer *node)
{
    int index, begin, length = 16 * 1024;
    int i, count = 0;

    if (node == NULL) return -1;

    if (node->peer_choking == 1 || node->am_interested == 0) return -1;

    Request_piece *p = node->Request_piece_head, *q = NULL;
    if (p != NULL) {
        while (p->next != NULL) { p = p->next; }
        int last_begin = piece_length - 16 * 1024;
        if (p->index == last_piece_index) {
            last_begin = (last_piece_count - 1) * 16 * 1024;
        }

        if (p->begin < last_begin) {
            index = p->index;
            begin = p->begin + 16 * 1024;
            count = 0;

            while (begin != piece_length && count < 1) {
                if (p->index == last_piece_index) {
                    if (begin == (last_piece_count - 1) * 16 * 1024)
                        length = last_slice_len;
                }
                create_request_msg(index, begin, length, node);
                q = (Request_piece *)malloc(sizeof(Request_piece));
                if (q == NULL) {
                    printf("%s:%d error\n", __FILE__, __LINE__);
                    return -1;
                }
                q->index  = index;
                q->begin  = begin;
                q->length = length;
                q->next   = NULL;
                p->next   = q;
                p         = q;
                begin += 16 * 1024;
                count++;
            }  // while
            return 0;
        }  // if
    }      // if

    if (get_rand_numbers(pieces_hash_length / 20) == -1) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < pieces_hash_length / 20; i++) {
        index = rand_num[i];
        if (get_bit_value(&(node->bitmap), index) != 1) continue;
        if (get_bit_value(bitmap, index) == 1) continue;
        Peer *         peer_ptr = peer_head;
        Request_piece *reqt_ptr;
        int            find = 0;
        while (peer_ptr != NULL) {
            reqt_ptr = peer_ptr->Request_piece_head;
            while (reqt_ptr != NULL) {
                if (reqt_ptr->index == index) {
                    find = 1;
                    break;
                }
                reqt_ptr = reqt_ptr->next;
            }
            if (find == 1) break;
            peer_ptr = peer_ptr->next;
        }
        if (find == 1) continue;
        break;
    }

    if (i == pieces_hash_length / 20) {
        if (end_mode == 0) end_mode = 1;
        for (i = 0; i < pieces_hash_length / 20; i++) {
            if (get_bit_value(bitmap, i) == 0) {
                index = i;
                break;
            }
        }
        if (i == pieces_hash_length / 20) {
            printf("Can not find an index to IP:%s\n", node->ip);
            return -1;
        }
    }
    begin = 0;
    count = 0;
    p     = node->Request_piece_head;
    if (p != NULL) {
        while (p->next != NULL) p = p->next;
        while (count < 4) {
            if (count + 1 > last_piece_count) break;
            if (begin == (last_piece_count - 1) * 16 * 1024)
                length = last_slice_len;
        }
        create_request_msg(index, begin, length, node);

        q = (Request_piece *)malloc(sizeof(Request_piece));
        if (q == NULL) {
            printf("%s:%d error\n", __FILE__, __LINE__);
            return -1;
        }
        q->index  = index;
        q->begin  = begin;
        q->length = length;
        q->next   = NULL;
        if (node->Request_piece_head == NULL) {
            node->Request_piece_head = q;
            p                        = q;
        } else {
            p->next = q;
            p       = q;
        }
        begin += 16 * 1024;
        count++;
    }
    if (rand_num != NULL) {
        free(rand_num);
        rand_num = NULL;
    }
    return 0;
}