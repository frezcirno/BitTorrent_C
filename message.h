#pragma once
#include "peer.h"
int int_to_char(int i, unsigned char c[4]);  // dec->hex(big-endian)
int char_to_int(unsigned char c[4]);         // hex->dec

int create_handshake_msg(char *info_hash, char *peer_id, Peer *peer);
int create_keep_alive_msg(Peer *peer);
int create_chock_interested_msg(int type, Peer *peer);
int create_have_msg(int index, Peer *peer);
int create_bitfield_msg(char *bitfield, int bitfield_len, Peer *peer);
int create_request_msg(int index, int begin, int length, Peer *peer);
int create_piece_msg(int index, int begin, char *clock, int b_len, Peer *peer);
int create_cancel_msg(int index, int begin, int length, Peer *peer);
int create_port_msg(int port, Peer *peer);
//当前消息是否是完整的一条消息
int is_complete_message(unsigned char *buff, unsigned int len, int *ok_len);

int process_handshake_msg(Peer *peer, unsigned char *buff, int len);
int process_keep_alive_msg(Peer *peer, unsigned char *buff, int len);
int process_choke_msg(Peer *peer, unsigned char *buff, int len);
int process_unchoke_msg(Peer *peer, unsigned char *buff, int len);
int process_interested_msg(Peer *peer, unsigned char *buff, int len);
int process_uninterested_msg(Peer *peer, unsigned char *buff, int len);
int process_have_msg(Peer *peer, unsigned char *buff, int len);
int process_bitfield_msg(Peer *peer, unsigned char *buff, int len);
int process_request_msg(Peer *peer, unsigned char *buff, int len);
int process_piece_msg(Peer *peer, unsigned char *buff, int len);
int process_cancel_msg(Peer *peer, unsigned char *buff, int len);

//处理消息队列
int parse_response(Peer *peer);
//处理最后一条不完整的消息队列(调用另一个函数)
int parse_response_uncomplete_msg(Peer *p, int ok_len);
int create_response_message(Peer *peer);
//广播have消息
int  prepare_send_have_msg();
void discard_send_buffer(Peer *peer);