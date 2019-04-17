#include "peer.h"
#include "bitfield.h"
#include "message.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

extern Bitmap *bitmap;

// peer列表头指针
Peer *peer_head;

int initialize_peer(Peer *peer)
{
    if (peer == NULL) return -1;
    peer->socket = -1;
    memset(peer->ip, 0, 16);
    peer->port = 0;
    memset(peer->id, 0, 21);
    peer->state = INITIAL;

    peer->in_buff      = NULL;
    peer->out_msg      = NULL;
    peer->out_msg_copy = NULL;

    peer->in_buff = (char *)malloc(MSG_SIZE);
    if (peer->out_msg == NULL) goto OUT;
    memset(peer->out_msg, 0, MSG_SIZE);
    peer->msg_len      = 0;
    peer->out_msg_copy = (char *)malloc(MSG_SIZE);
    if (peer->out_msg_copy == NULL) goto OUT;
    memset(peer->out_msg_copy, 0, MSG_SIZE);
    peer->msg_copy_len   = 0;
    peer->msg_copy_index = 0;

    peer->am_choking      = 1;
    peer->am_interested   = 0;
    peer->peer_choking    = 1;
    peer->peer_interested = 0;
    // initialize bitmap
    peer->bitmap.bitfield        = NULL;
    peer->bitmap.bitfield_length = 0;
    peer->bitmap.valid_length    = 0;

    peer->Requested_piece_head = NULL;
    peer->Requested_piece_head = NULL;

    peer->down_total = 0;
    peer->up_total   = 0;

    peer->start_timestamp = 0;
    peer->recet_timestamp = 0;

    peer->last_down_timestamp = 0;
    peer->last_up_timestamp   = 0;
    peer->down_count          = 0;
    peer->up_count            = 0;
    peer->down_rate           = 0;
    peer->up_rate             = 0;

    peer->next = (Peer *)0;
    return 0;
OUT:
    if (peer->in_buff) free(peer->in_buff);
    if (peer->out_msg) free(peer->out_msg);
    if (peer->out_msg_copy) free(peer->out_msg_copy);
    return -1;
}
Peer *add_peer_node()
{
    int   ret;
    Peer *node, *p;
    // new
    node = (Peer *)malloc(sizeof(Peer));
    if (node == NULL) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return NULL;
    }
    // init
    ret = initialize_peer(node);
    if (ret < 0) {
        printf("%s:%d error\n", __FILE__, __LINE__);
        return NULL;
    }
    // add to peer list
    if (peer_head == NULL) {
        peer_head = node;
    } else {
        p = peer_head;
        while (p->next != NULL) p = p->next;
        p->next = node;
    }
    return node;
}

int del_peer_node(Peer *peer)
{
    Peer *p = peer_head, *q;
    if (peer == NULL) return -1;

    while (p != NULL) {
        if (p == peer) {         // find it
            if (p == peer_head)  // first
                peer_head = p->next;
            else
                q->next = p->next;
            free_peer_node(p);
            return 0;
        } else {    // find next
            q = p;  // let q->next==p
            p = p->next;
        }
    }
    return -1;
}
int cancel_request_list(Peer *node)
{
    Request_piece *p = node->Request_piece_head;
    while (p != NULL) {
        node->Request_piece_head = node->Request_piece_head->next;
        free(p);
        p = node->Request_piece_head;
    }
    return 0;
}
int cancel_requested_list(Peer *node)
{
    Request_piece *p = node->Requested_piece_head;
    while (p != NULL) {
        node->Requested_piece_head = node->Requested_piece_head->next;
        free(p);
        p = node->Requested_piece_head;
    }
    return 0;
}
void free_peer_node(Peer *node)
{
    if (node == NULL) return;
    if (node->in_buff) free(node->in_buff);
    if (node->out_msg) free(node->out_msg);
    if (node->out_msg_copy) free(node->out_msg_copy);
}
void release_memory_in_peer()
{
    Peer *p;

    if (peer_head == NULL) return;

    p = peer_head;
    while (p != NULL) {
        peer_head = peer_head->next;
        free_peer_node(p);
        p = peer_head;
    }
}