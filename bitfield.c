/**
 * 字节内顺序:
 * char:      char[0]    |    char[1]      |    char[2]       | ...
 * Bit:  0 0 0 0 0 0 0 0 | 0 0 0 0 0 0 0 0 | 0 0 0 0 0 0 0 0  | ...
 * index:7 6 5 4 3 2 1 0 | 5 4 3 2 1 10 9 8| 3 2 1 20 9 8 7 6 | ...
 * **/

#include "bitfield.h"
#include "parse_metafile.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int   pieces_hash_length;
extern char *file_name;

//己方下载的位图
Bitmap *bitmap = NULL;
//下载piece总数
int download_piece_num = 0;

int create_bitfield()
{
    //创建位图
    bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (bitmap == NULL) {
        printf("allocate memory for bitmap failed.\n");
        return -1;
    }
    //计算位图所需大小
    bitmap->valid_length    = pieces_hash_length / 20;
    bitmap->bitfield_length = pieces_hash_length / 20 / 8;
    if (pieces_hash_length / 20 % 8 != 0) bitmap->bitfield_length++;

    //根据计算结果分配位图空间
    bitmap->bitfield = (unsigned char *)malloc(bitmap->bitfield_length);
    if (bitmap->bitfield == NULL) {
        printf("allocate memory for bitmap->bitfield failed\n");
        if (bitmap != NULL) free(bitmap);
        return -1;
    }

    char  bitmapfile[64];
    int   i;
    FILE *fp;

    sprintf(bitmapfile, "%dbitmap", pieces_hash_length);
    fp = fopen(bitmapfile, "rb");
    if (fp == NULL) {
        //重置位图
        memset(bitmap->bitfield, 0, bitmap->bitfield_length);
    } else {
        //从文件中恢复
        fseek(fp, 0, SEEK_SET);
        for (i = 0; i < bitmap->bitfield_length; i++) {
            (bitmap->bitfield)[i] = fgetc(fp);
        }
        fclose(fp);
        //更新已下载情况
        download_piece_num = get_download_piece_num();
    }
#ifdef DEBUG
    printf("Bitmap: \n");
    for (int i = 0; i < bitmap->bitfield_length; i++)
        printf("%d|", bitmap->bitfield[i]);
    printf("\n");
#endif

    return 0;
}

int get_bit_value(Bitmap *bitmap, int index)
{
    int           byte_index;
    unsigned char byte_value;
    unsigned char inner_byte_index;

    if (bitmap == NULL || index < 0 || index >= bitmap->valid_length) return -1;

    byte_index       = index / 8;
    inner_byte_index = index % 8;

    byte_value = bitmap->bitfield[byte_index];
    byte_value >>= inner_byte_index;

    return byte_value & 0x1;
}

int set_bit_value(Bitmap *bitmap, int index, unsigned char value)
{
    int            byte_index;
    unsigned char *byte_value;
    unsigned char  inner_byte_index;

    if (bitmap == NULL || index < 0 || index >= bitmap->valid_length) return -1;

    byte_index       = index / 8;
    inner_byte_index = index % 8;

    byte_value = bitmap->bitfield + byte_index;
    *byte_value |= (0x1 << inner_byte_index);

    return 0;
}

int reset(Bitmap *bitmap)
{
    if (bitmap->bitfield == NULL) return -1;  // validity check
    memset(bitmap->bitfield, 0, bitmap->bitfield_length);
    return 0;
}

int all_set(Bitmap *bitmap)
{
    if (bitmap->bitfield == NULL) return -1;  // validity check
    memset(bitmap->bitfield, 1, bitmap->bitfield_length);
    return 0;
}

void release_memory_in_bitfield()
{
    if (bitmap->bitfield != NULL) free(bitmap->bitfield);
    if (bitmap != NULL) free(bitmap);
}

//保存位图,支持断点续传
int restore_bitmap()
{
    int  fd;              //文件描述符
    char bitmapfile[64];  //缓冲区
    if (bitmap == NULL || file_name == NULL) return -1;
    sprintf(bitmapfile, "%dbitmap", pieces_hash_length);
    fd = open(bitmapfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return -1;
    write(fd, bitmap->bitfield /*content*/, bitmap->bitfield_length /*length*/);
    return 0;
}
int is_interested(Bitmap *dst, Bitmap *src)
{
    unsigned char mask_num[8] = {
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    unsigned char c1, c2;
    int           i, j;

    if (dst == NULL || src == NULL) return -1;
    if (dst->bitfield == NULL || src->bitfield == NULL) return -1;
    if (dst->bitfield_length != src->bitfield_length ||
        dst->valid_length != src->valid_length)
        return -1;

    for (i = 0; i < dst->bitfield_length - 1; i++) {
        for (j = 0; j < 8; j++) {
            c1 = (dst->bitfield)[i] & mask_num[j];
            c2 = (src->bitfield)[i] & mask_num[j];
            // src有该piece,而dst没有该piece
            if (c1 > 0 && c2 == 0) return 1;
        }
    }
    //最后一个字节
    j  = dst->valid_length % 8;
    c1 = dst->bitfield[dst->bitfield_length - 1];
    c2 = src->bitfield[src->bitfield_length - 1];
    for (i = 0; i < j; i++) {
        if ((c1 & mask_num[i]) > 0 && (c2 & mask_num[i]) == 0) return 1;
    }
    return 0;
}
int get_download_piece_num()
{
    int           b_idx, in_idx;
    unsigned char byte_value;
    //每一位置1的数值
    unsigned char mask_num[8] = {1, 2, 4, 8, 16, 32, 64, 128};

    if (bitmap == NULL || bitmap->bitfield == NULL) return 0;

    download_piece_num = 0;
    for (b_idx = 0; b_idx < bitmap->bitfield_length - 1; b_idx++)
        for (in_idx = 0; in_idx < 8; in_idx++)
            if (((bitmap->bitfield)[b_idx] & mask_num[in_idx]) != 0)
                download_piece_num++;

    byte_value = bitmap->bitfield[b_idx];
    in_idx     = bitmap->valid_length % 8;
    for (b_idx = 0; b_idx < in_idx; b_idx++)
        if ((byte_value & mask_num[b_idx]) != 0) download_piece_num++;

    return download_piece_num;
}