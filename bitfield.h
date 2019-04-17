#pragma once
typedef struct _Bitmap {
    // 1 char = 1 Byte = 8 bits
    unsigned char *bitfield;
    // 位图长度(字节数)
    int bitfield_length;
    // 有效位数(bit数)
    int valid_length;
    //位图,用来标记已下载过哪些piece
} Bitmap;
//创建位图(自己的)
int create_bitfield();
//获取某一位的值
int get_bit_value(Bitmap *bitmap, int index);
//设置某一位的值
int set_bit_value(Bitmap *bitmap, int index, unsigned char value);

//重置为全0
int reset(Bitmap *bitmap);
//全置为1
int all_set(Bitmap *bitmap);
//释放内存,收尾工作
void release_memory_in_bitfield();

//将位图存到文件中,实现暂停、继续下载的功能
int restore_bitmap();

//有src的peer是否对有dst的peer感兴趣(想要下载)
int is_interested(Bitmap *dst, Bitmap *src);

//获取已下载总piece数
int get_download_piece_num();
