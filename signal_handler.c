#include "signal_handler.h"
#include "bitfield.h"
#include "data.h"
#include "parse_metafile.h"
#include "peer.h"
#include "torrent.h"
#include "tracker.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int   download_piece_num;
extern int * fds;
extern int   fds_len;
extern Peer *peer_head;

void do_clear_work()
{
    Peer *p = peer_head;
    int   i;

    //关闭所有socket文件
    while (p != NULL) {
        if (p->state != CLOSING) close(p->socket);
        p = p->next;
    }
    //保存位图
    if (download_piece_num > 0) restore_bitmap();
    //关闭文件描述符
    for (i = 0; i < fds_len; i++) close(fds[i]);
    //释放内存
    release_memory_in_parse_metafile();
    release_memory_in_bitfield();
    release_memory_in_btcache();
    release_memory_in_peer();
    release_memory_in_torrent();

    exit(0);
}
void process_signal(int signo)
{
    printf("Error no:%d. Please wait for clear operations\n", signo);
    do_clear_work();
}
int set_signal_handler()
{
    // signal(int sig, void(*signal_handler)(int) )
    // 捕捉sig类型的错误信号,并调用signal_handler函数
    // return type: void(*)(int) , the last signal_handler

    /* Broken pipe.  -> IGNORE*/
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("can not catch signal:sigpipe\n");
        return -1;
    }
    /* Interactive attention signal.  -> do_clear_work()*/
    if (signal(SIGINT, process_signal) == SIG_ERR) {
        perror("can not catch signal:sigint\n");
        return -1;
    }
    /* Termination request.  -> do_clear_work()*/
    if (signal(SIGTERM, process_signal) == SIG_ERR) {
        perror("can not catch signal:sigterm\n");
        return -1;
    }
    return 0;
}