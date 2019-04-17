#include "bitfield.h"
#include "data.h"
#include "log.h"
#include "parse_metafile.h"
#include "policy.h"
#include "signal_handler.h"
#include "torrent.h"
#include "tracker.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// #define DEBUG

int main(int argc, char const *argv[])
{
    int ret;
    if (argc != 2) {
        printf("usage:%s metafile\n", argv[0]);
        exit(-1);
    }

    //设置信号处理函数
    ret = set_signal_handler();
    if (ret != 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    //解析种子文件
    ret = parse_metafile(argv[1]);
    if (ret != 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    //创建文件
    ret = create_files();
    if (ret != 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    ret = create_bitfield();
    if (ret != 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    ret = create_btcache();
    if (ret != 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    init_unchoke_peers();

    download_upload_with_peers();
    printf("%s:%d OK\n", __FILE__, __LINE__);

    do_clear_work();
    printf("%s:%d OK\n", __FILE__, __LINE__);

    return 0;
}
