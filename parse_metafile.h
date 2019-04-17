#pragma once

typedef struct _Announce_list {
    char                   announce[128];
    struct _Announce_list *next;
    //保存种子文件中提供的tracker的URL
} Announce_list;
typedef struct _Files {
    char           path[256];
    long           length;
    struct _Files *next;
    //保存各个文件路径和长度
} Files;

//读取种子文件
int read_metafile(char *metafile_name);
//查找关键词
int find_keyword(char *keyword, long *position);
//获取tracker地址
int read_announce_list();
//向tracker列表添加URL
int add_an_announce(char *url);

//获取每个piece的长度(default 256KB)
int get_piece_length();
//获取各个piece的hash值
int get_pieces_hash();

//判断是否下载多文件
int is_multi_files();
//获取文件名(多文件时为目录名)
int get_file_name();
//获取文件总长度
int get_file_length();
//(多文件)获取文件路径和长度
int get_files_length_path();
//获取文件数量
int get_files_count();

//计算整个info字段的hash值
int get_info_hash();
//生成一个peer_id
int get_peer_id();

//释放用到的内存
void release_memory_in_parse_metafile();
//主函数
int parse_metafile(char *metafile);
