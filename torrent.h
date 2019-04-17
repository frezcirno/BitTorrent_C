#pragma once
// torrent.c
//负责数据交换

//主函数,维持循环
int download_upload_with_peers();

//打印peer链表中各个peer的信息
int print_peer_list();
//打印下载进度消息
void print_process_info();
//释放连接tracker的内存
void clear_connect_tracker();
//释放连接peer的内存
void clear_connect_peer();
//释放处理tracker消息的内存
void clear_tracker_response();
//善后工作
void release_memory_in_torrent();
