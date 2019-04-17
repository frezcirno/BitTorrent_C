#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "bitfield.h"
#include "data.h"
#include "message.h"
#include "parse_metafile.h"
#include "peer.h"
#include "policy.h"
#include "torrent.h"
#include "tracker.h"

//缓冲区阈值,缓冲区大小18KB-一个数据包最大长度1500B
#define threshold (18 * 1024 - 1500)

// tracker的URL链表
extern Announce_list *announce_list_head;
extern char *         file_name;
extern long long      file_length;
extern int            piece_length;
extern char *         pieces_hash;
extern int            pieces_hash_length;
extern Peer *         peer_head;

extern long long  total_down, total_up;
extern float      total_down_rate, total_up_rate;
extern int        total_peers;
extern int        download_piece_num;
extern Peer_addr *peer_addr_head;

//连接tracker的文件描述符
int *sock = NULL;
//连接tracker时使用,socket的标识符
struct sockaddr_in *tracker = NULL;
//所连接tracker的状态
// 0->socket创建失败或获取不到host或者连接出错
//-1->已获取hostname
// 1->连接成功
int *valid = NULL;
// tracker服务器的个数
int tracker_count = 0;

//连接peer的socket
int *peer_sock = NULL;
//连接peer用
struct sockaddr_in *peer_addr = NULL;
// peer的状态
int *peer_valid = NULL;
//尝试建立连接的peer的数量
int peer_count = 0;

// tracker的回应
char *tracker_response = NULL;
//长度
int response_len = 0;
//当前长度
int response_index = 0;

int download_upload_with_peers()
{
    Peer *p;
    int   ret, max_sockfd, i;

    int    connect_tracker;     //是否需要连接tracker
    int    connecting_tracker;  //是否正在连接tracker
    int    connect_peer;        //是否需要连接peer
    int    connecting_peer;     //是否正在连接peer
    time_t last_time[3], now_time;

    //开始连接tracker的时间
    time_t start_connect_tracker;
    //开始连接peer的时间
    time_t start_connect_peer;
    // select要监视的描述符集合set
    fd_set rset, wset;
    // select函数的超时时间, struct timeval为高精度时间戳
    struct timeval tmval;

    now_time        = time(NULL);
    last_time[0]    = now_time;  //上一次选择非阻塞peer的时间
    last_time[1]    = now_time;  //上一次选择优化非阻塞peer的时间
    last_time[2]    = now_time;  //上一次选择连接tracker的时间
    connect_tracker = 1;
    connecting_tracker = 0;
    connect_peer       = 0;
    connecting_peer    = 0;

    while (1) {
        // printf("Restart :: c_trker: %d | cing_trker: %d | c_peer: %d | "
        //        "cing_peer: %d\n",
        //     connect_tracker, connecting_tracker, connect_peer,
        //     connecting_peer);

        max_sockfd = 0;
        now_time   = time(NULL);

        // 10秒重选一次unchoke peer
        if (now_time - last_time[0] >= 10) {
            if (download_piece_num > 0 && peer_head != NULL) {
                //计算上传下载速度
                compute_rate();
                //选择非阻塞peer
                select_unchoke_peer();
                last_time[0] = now_time;
            }
        }

        // 30秒重选一次优化unchoke peer
        if (now_time - last_time[1] >= 30) {
            if (download_piece_num > 0 && peer_head != NULL) {
                select_optunchoke_peer();
                last_time[1] = now_time;
            }
        }

        // 5分钟(或需要时)连接一次tracker,状态变为connecting_tracker
        if ((now_time - last_time[2] >= 300 || connect_tracker == 1) &&
            connecting_tracker != 1 && connect_peer != 1 &&
            connecting_peer != 1) {
            ret = prepare_connect_tracker(&max_sockfd);
            if (ret < 0) {
                printf("[E]prepare_connect_tracker failed\n");
                return -1;
            }
            connect_tracker       = 0;
            connecting_tracker    = 1;
            start_connect_tracker = now_time;
        }

        //(立即)如果需要连接peer,准备连接之
        if (connect_peer == 1) {
            printf("[*]Prepare to connect peers\n");
            ret = prepare_connect_peer(&max_sockfd);
            if (ret < 0) {
                printf("[E]prepare_connect_peer failed\n");
                return -1;
            }
            printf("[I]Prepare to connect peers Success\n");
            connect_peer       = 0;
            connecting_peer    = 1;
            start_connect_peer = now_time;
        }

        //(立即)将两个set清零
        FD_ZERO(&rset);
        FD_ZERO(&wset);

        //(立即)将连接tracker的socket加入监视
        //成功->准备连接peer
        if (connecting_tracker == 1) {
            int flag = 1;  //没有valid等于-1,1,2的sock
            //如果连接超过10s,则不再连接tracker,关闭所有socket
            if (now_time - start_connect_tracker > 10) {
                for (i = 0; i < tracker_count; i++)
                    if (valid[i] != 0) close(sock[i]);
            } else {
                //否则,找到最大的socket
                for (i = 0; i < tracker_count; i++) {
                    //更新最大sockfd的值
                    if (valid[i] != 0 && sock[i] > max_sockfd)
                        max_sockfd = sock[i];
                    //监视valid等于-1,1,2的sock
                    if (valid[i] == -1) {
                        FD_SET(sock[i], &rset);
                        FD_SET(sock[i], &wset);
                        flag = 0;
                    } else if (valid[i] == 1) {
                        FD_SET(sock[i], &wset);
                        flag = 0;
                    } else if (valid[i] == 2) {
                        FD_SET(sock[i], &rset);
                        flag = 0;
                    }
                }
            }
            //如果没有要监视的,说明连接tracker过程已结束,可以真正与peer连接
            if (flag == 1) {
                connecting_tracker = 0;
                last_time[2]       = now_time;
                clear_connect_tracker();
                clear_tracker_response();
                //已经获取peer信息,准备连接peer
                if (peer_addr_head != NULL) {
                    connect_tracker = 0;
                    connect_peer    = 1;
                } else
                    //没有则重来
                    connect_tracker = 1;
                continue;
            }
        }

        //(立即)将正在连接的peer的socket加入监视
        if (connecting_peer == 1) {
            int flag = 1;
            //连接peer超过10s,说明网络断开,不再连接peer
            if (now_time - start_connect_peer > 10) {
                for (i = 0; i < peer_count; i++) {
                    if (peer_valid[i] == -1) close(peer_sock[i]);
                }
            } else {
                for (i = 0; i < peer_count; i++) {
                    //已经建立socket,加入监视
                    if (peer_valid[i] == -1) {
                        if (peer_valid[i] > max_sockfd)
                            max_sockfd = peer_sock[i];
                        FD_SET(peer_sock[i], &rset);
                        FD_SET(peer_sock[i], &wset);
                        if (flag == 1) flag = 0;
                    }
                }
            }

            //如果没有要监视的(网络状况),说明连接peer过程已结束
            if (flag == 1) {
                printf("[I]No new peers\n");
                connecting_peer = 0;
                clear_connect_peer();
                //没有获取到peer,重新开始循环,连接tracker
                if (peer_head == NULL) connect_tracker = 1;
                continue;
            }
        }

        //将已经连接的peer的socket加入监视
        connect_tracker = 1;
        //监视可用的peer,若没有->重新连接tracker
        p = peer_head;
        while (p != NULL) {
            if (p->state != CLOSING && p->socket > 0) {
                FD_SET(p->socket, &rset);
                FD_SET(p->socket, &wset);
                if (p->socket > max_sockfd) max_sockfd = p->socket;
                connect_tracker = 0;
            }
            p = p->next;
        }
        //第一次的时候不检测
        if (peer_head == NULL &&
            (connecting_tracker == 1 || connecting_peer == 1))
            connect_tracker = 0;
        //需重新连接tracker
        if (connect_tracker == 1) {
            printf("[--]No tracker, reconnect\n");
            continue;
        }

        //调用select函数监视各个socket状态
        tmval.tv_sec  = 2;
        tmval.tv_usec = 0;
        ret           = select(max_sockfd + 1, &rset, &wset, NULL, &tmval);
        if (ret < 0) {
            printf("%s:%d error", __FILE__, __LINE__);
            perror("select error");
            break;
        } else if (ret == 0) {  // select超时,失败重来
            printf("[-]Select timeout wait\n");
            continue;
        }
        //单独处理have广播消息
        prepare_send_have_msg();
        //处理每个peer完整的消息
        p = peer_head;
        while (p != NULL) {
            printf("[I]Peer:%s, State:%d", p->ip, p->state);
            if (p->state != CLOSING && FD_ISSET(p->socket, &rset)) {
                ret = recv(p->socket, p->in_buff + p->buff_len,
                    MSG_SIZE - p->buff_len, 0);
                printf("[*]Peer %s readable\n", p->ip);
                //断连了
                if (ret <= 0) {
                    printf("[-]Peer %s closed\n", p->id);
                    p->state = CLOSING;
                    discard_send_buffer(p);
                    clear_btcache_before_peer_close(p);
                    close(p->socket);
                } else {
                    //处理消息
                    int completed, ok_len;
                    p->buff_len += ret;
                    completed =
                        is_complete_message(p->in_buff, p->buff_len, &ok_len);
                    if (completed == 1)
                        parse_response(p);
                    else if (p->buff_len >=
                             threshold)  //消息缓冲区已满,收到的消息不完整
                        parse_response_uncomplete_msg(p, ok_len);
                    else
                        p->start_timestamp = time(NULL);
                }
            }  // if
            if (p->state != CLOSING && FD_ISSET(p->socket, &wset)) {
                if (p->msg_len > 0) {
                    memcpy(p->out_msg_copy, p->out_msg, p->msg_len);
                    p->msg_copy_len = p->msg_len;
                    p->msg_len      = 0;
                }
                if (p->msg_copy_len > 1024) {
                    send(p->socket, p->out_msg_copy + p->msg_copy_index, 1024,
                        0);
                    p->msg_copy_len    = p->msg_copy_len - 1024;
                    p->msg_copy_index  = p->msg_copy_index + 1024;
                    p->recet_timestamp = time(NULL);
                } else if (p->msg_copy_len <= 1024 && p->msg_copy_len > 0) {
                    send(p->socket, p->out_msg_copy + p->msg_copy_index,
                        p->msg_copy_len, 0);
                    p->msg_copy_len    = 0;
                    p->msg_copy_index  = 0;
                    p->recet_timestamp = time(NULL);
                }
            }  // if
            p = p->next;
        }

        //检查tracker的socket是否有消息
        if (connecting_tracker == 1) {
            for (i = 0; i < tracker_count; i++) {
                if (valid[i] == -1) {
                    if (FD_ISSET(sock[i], &wset)) {
                        int error, len;
                        error = 0;
                        len   = sizeof(error);
                        ret   = getsockopt(
                            sock[i], SOL_SOCKET, SO_ERROR, &error, &len);
                        if (ret < 0) {
                            valid[i] = 0;
                            close(sock[i]);
                        }
                        if (error) {
                            valid[i] = 0;
                            close(sock[i]);
                        } else
                            valid[i] = 1;
                    }
                }
                if (valid[i] == 1 && FD_ISSET(sock[i], &wset)) {
                    char           request[1024];
                    unsigned short listen_port = 33550;
                    unsigned long  down        = total_down;
                    unsigned long  up          = total_up;
                    unsigned long  left;
                    left = (pieces_hash_length / 20 - download_piece_num) *
                           piece_length;

                    int            num    = i;
                    Announce_list *anouce = announce_list_head;
                    while (num > 0) {
                        anouce = anouce->next;
                        num--;
                    }
                    create_request(request, 1024, anouce, listen_port, down, up,
                        left, 200);
                    write(sock[i], request, strlen(request));
                    valid[i] = 2;
                    printf("[+]Tracker request sent\n");
                }
                if (valid[i] == 2 && FD_ISSET(sock[i], &rset)) {
                    char buffer[2048];
                    char redirection[128];
                    ret = read(sock[i], buffer, sizeof(buffer));
                    if (ret > 0) {
                        if (response_len != 0) {
                            memcpy(
                                tracker_response + response_index, buffer, ret);
                            response_index += ret;
                            if (response_index == response_len) {
                                parse_tracker_response2(
                                    tracker_response, response_len);
                                clear_tracker_response();
                                valid[i] = 0;
                                close(sock[i]);
                                last_time[2] = time(NULL);
                            }
                        } else if (get_response_type(
                                       buffer, ret, &response_len) == 1) {
                            tracker_response = (char *)malloc(response_len);
                            if (tracker_response == NULL)
                                printf("malloc error\n");
                            memcpy(tracker_response, buffer, ret);
                            response_index = ret;
                        } else {
                            ret = parse_tracker_response1(
                                buffer, ret, redirection, 128);
                            if (ret == 1) add_an_announce(redirection);
                            valid[i] = 0;
                            close(sock[i]);
                            last_time[2] = time(NULL);
                        }  // if
                    }      // if
                }          // if
            }              // for i
        }                  // if

        //检查新peer的socket是否已连接
        if (connecting_peer == 1) {
            for (i = 0; i < peer_count; i++) {
                if (peer_valid[i] == -1 && FD_ISSET(peer_sock[i], &wset)) {
                    int error = 0;
                    int len   = sizeof(error);
                    ret       = getsockopt(
                        peer_sock[i], SOL_SOCKET, SO_ERROR, &error, &len);
                    if (ret < 0) peer_valid[i] = 0;
                    if (error == 0) {
                        peer_valid[i] = -1;
                        add_peer_node_to_peerlist(&peer_sock[i], peer_addr[i]);
                    }
                }
            }
        }

        //将已断开连接的peer删除
        p = peer_head;
        while (p != NULL) {
            if (p->state == CLOSING) {
                del_peer_node(p);
                p = peer_head;
            } else
                p = p->next;
        }

        //判断下载是否完毕
        if (download_piece_num == piece_length / 20) {
            printf("++++++ All Files Downloaded Successfully +++++\n");
            break;
        }
    }  // while(1)

    return 0;
}

void clear_connect_tracker()
{
    if (sock != NULL) {
        free(sock);
        sock = NULL;
    }
    if (tracker != NULL) {
        free(tracker);
        tracker = NULL;
    }
    if (valid != NULL) {
        free(valid);
        valid = NULL;
    }
    tracker_count = 0;
}
void clear_connect_peer()
{
    if (peer_sock != NULL) {
        free(peer_sock);
        peer_sock = NULL;
    }
    if (peer_addr != NULL) {
        free(peer_addr);
        peer_addr = NULL;
    }
    if (peer_valid != NULL) {
        free(peer_valid);
        peer_valid = NULL;
    }
    peer_count = 0;
}
void clear_tracker_response()
{
    if (tracker_response != NULL) {
        free(tracker_response);
        tracker_response = NULL;
    }
    response_len   = 0;
    response_index = 0;
}
void release_memory_in_torrent()
{
    if (sock != NULL) {
        free(sock);
        sock = NULL;
    }
    if (tracker != NULL) {
        free(tracker);
        tracker = NULL;
    }
    if (valid != NULL) {
        free(valid);
        valid = NULL;
    }

    if (peer_sock != NULL) {
        free(peer_sock);
        peer_sock = NULL;
    }
    if (peer_addr != NULL) {
        free(peer_addr);
        peer_addr = NULL;
    }
    if (peer_valid != NULL) {
        free(peer_valid);
        peer_valid = NULL;
    }
    free_peer_addr_head();
}