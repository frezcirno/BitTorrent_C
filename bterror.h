#pragma once
#define FILE_FD_ERR
#define FILE_READ_ERR
#define FILE_WRITE_ERR
#define INVALID_METAFILE_ERR
#define INVALID_SOCKET_ERR
#define INVALID_TRACKER_URL_ERR
#define INVALID_TRACKER_REPLY_ERR
#define INVALID_HASH_ERR
#define INVALID_MESSAGE_ERR
#define INVALID_PARAMETER_ERR
#define FAILED_ALLOCATE_MEM_ERR
#define NO_BUFFER_ERR
#define READ_SOCKET_ERR
#define WRITE_SOCKET_ERR
#define RECEIVE_EXIT_SIGNAL_ERR

void btexit(int error, char* file, int line);