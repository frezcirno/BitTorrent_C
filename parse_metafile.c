#include "parse_metafile.h"
#include "sha1.h"
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//从metafile_content中读入一个数字
#define GET_NUM(split, saveto)                                                 \
    while (metafile_content[i] != split) {                                     \
        saveto = saveto * 10 + metafile_content[i] - '0';                      \
        i++;                                                                   \
    }                                                                          \
    i++
//存储种子文件的内容
char *metafile_content = NULL;
//种子文件的大小
long filesize;

//每个piece的大小,在种子文件中标明,通常为262144B,即256K
int piece_length = 0;
//存放每个piece的hash值,对应种子文件中的pieces字段
char *pieces_hash = NULL;
// piece的hash值长度
int pieces_hash_length = 0;

//单文件or多文件
int multi_file = -1;
//存放文件名或目录名
char *file_name = NULL;
//文件总长度(最大9223372036854775807KB)
long long file_length = 0;
//(多文件)存放各个文件路径和长度
Files *files_head = NULL;
//(多文件)文件个数
int file_count = -1;
//保存info_hash的值
unsigned char info_hash[20];
//保存peer_id的值
unsigned char peer_id[21];

//保存所有tracker的URL
Announce_list *announce_list_head = NULL;

//打开种子文件,filesize
int read_metafile(char *metafile_name)
{
    long i;

    //打开种子文件,用二进制是因为种子文件可能包含0x00
    FILE *fp = fopen(metafile_name, "rb");
    if (!fp) {
        printf("%s:%d can not open file.\n", __FILE__, __LINE__);
        return -1;
    }
    //获取文件大小
    fseek(fp, 0, SEEK_END);  // fseek(fp,offset,from):使fp移动到from + offset
    filesize = ftell(fp);    // ftell(fp):返回当前fp的位置
    if (filesize == -1) {
        printf("%s:%d fseek failed.\n", __FILE__, __LINE__);
        return -1;
    }
    //分配空间
    metafile_content = (char *)malloc(filesize + 1);
    if (metafile_content == NULL) {
        printf("%s:%d malloc failed.\n", __FILE__, __LINE__);
        return -1;
    }
    //保存文件
    fseek(fp, 0, SEEK_SET);  // SEEK_SET代表开头
    for (i = 0; i < filesize; i++) metafile_content[i] = fgetc(fp);
    /*fgetc(fp):读取一个字符并将fp后移*/
    metafile_content[i] = '\0';

    fclose(fp);

#ifdef DEBUG
    printf("metafile size is: %ld\n", filesize);
#endif
    return 0;
}
//查找关键字
int find_keyword(char *keyword, long *position)
{
    long i;

    *position = -1;
    if (keyword == NULL) return -1;

    for (i = 0; i < filesize - strlen(keyword); i++) {
        if (memcmp(&metafile_content[i], keyword, strlen(keyword)) == 0) {
            /*find it!*/
            *position = i;
            return 1;
        }
    }
    return 0;
}

int read_announce_list()
{
    Announce_list *node = NULL, *p = NULL;
    int            len = 0;
    long           i;

    if (find_keyword("13:announce-list", &i) == 0) {
        /*type: single announce"*/
        if (find_keyword("8:announce", &i) == 1) {
            /*found announce*/
            i += 10;
            /*read URL length*/
            GET_NUM(':', len);

            /*insert new announce to list*/
            node = (Announce_list *)malloc(sizeof(Announce_list));
            strncpy(node->announce, &metafile_content[i], len);
            node->announce[len] = '\0';
            node->next          = NULL;
            announce_list_head  = node;
        }
    } else {
        /*type: announce-list"*/
        i += 16;
        i++;  // pass over 'l'
        while (metafile_content[i] != 'e') {
            i++;  // pass over 'l'
            /*read URL length*/
            while (isdigit(metafile_content[i])) {
                len = len * 10 + (metafile_content[i] - '0');
                i++;
            }
            if (metafile_content[i] == ':')
                i++;  // pass over ':'
            else
                return -1;
            /*process http link*/
            if (memcmp(&metafile_content[i], "http", 4) == 0) {
                /*found http link*/
                node = (Announce_list *)malloc(sizeof(Announce_list));
                strncpy(node->announce, &metafile_content[i], len);
                node->announce[i] = '\0';
                node->next        = NULL;

                /*insert link node*/
                if (announce_list_head == NULL)
                    announce_list_head = node;
                else {
                    p = announce_list_head;
                    while (p->next) p = p->next;
                    p->next = node;
                }
            }

            i += len;
            len = 0;
            i++;  // pass over 'e' (end flag)
            if (i >= filesize) return -1;

        }  // while (metafile_content[i] != 'e')
    }
#ifdef DEBUG
    p = announce_list_head;
    printf("announce list\n");
    while (p) {
        printf("%s\n", p->announce);
        p = p->next;
    }
#endif
    return 0;
}
/**
 * function: insert new url into the list
 * return:
 *  already exist:0
 *  success:1
 * **/
int add_an_announce(char *url)
{
    Announce_list *p = announce_list_head, *q;
    /*check if exists*/
    while (p) {
        if (strcmp(p->announce, url) == 0) break;  // already exist
        p = p->next;
    }
    if (p) return 0;  // already exist

    /*allocate new node*/
    q = (Announce_list *)malloc(sizeof(Announce_list));
    strcpy(q->announce, url);
    q->next = NULL;

    p = announce_list_head;
    if (p == NULL) {
        /*empty announce list*/
        /*insert it*/
        announce_list_head = q;
        return 1;
    }
    /*find list end*/
    while (p->next != NULL) { p = p->next; }
    /*insert it*/
    p->next = q;
#ifdef DEBUG
    printf("[+]Insert tracker to announce_list:%s\n", q->announce);
    printf("[I]Now length:%d\n", q->announce);
#endif
    return 1;
}
/**
 * function:get piece length, save it to global-piece_length
 * **/
int get_piece_length()
{
    long i;
    if (find_keyword("12:piece length", &i) == 1) {
        i += 15;
        i++;  // pass over 'i'
        GET_NUM('e', piece_length);
    } else {
        return -1;
    }
#ifdef DEBUG
    printf("piece length:%d\n", piece_length);
#endif
    return 0;
}

/**
 * function:save all hashes of pieces_hash into pieces_hash
 * return:
 *  success:0
 *  failed:-1
 * **/
int get_pieces_hash()
{
    long i;
    if (find_keyword("6:pieces", &i) == 1) {
        i += 8;
        GET_NUM(':', pieces_hash_length);
        pieces_hash = (char *)malloc(pieces_hash_length + 1);
        memcpy(pieces_hash, &metafile_content[i], pieces_hash_length);
        pieces_hash[pieces_hash_length] = '\0';
    } else {
        return -1;
    }
#ifdef DEBUG
    printf("Get pieces hash OK\n");
#endif
    return 0;
}

int is_multi_files()
{
    long i;
    if (multi_file == -1) {
        if (find_keyword("5:files", &i) == 1)
            multi_file = 1;
        else
            multi_file = 0;
    }
    return multi_file;
}

int get_file_name()
{
    long i;
    int  count = 0;

    if (find_keyword("4:name", &i) == 1) {
        i += 6;
        GET_NUM(':', count);
        file_name = (char *)malloc(count + 1);
        memcpy(file_name, metafile_content + i, count);
        file_name[count] = '\0';
    } else
        return -1;

#ifdef DEBUG
    printf("file(directory)_name: %s\n", file_name);
#endif
    return 0;
}
/**
 * function: get size of the file to be download, save it to (global)file_length
 * **/
int get_file_length()
{
    long i;
    if (is_multi_files()) {
        if (files_head == NULL) get_files_length_path();
        Files *p = files_head;
        while (p) {
            file_length += p->length;
            p = p->next;
        }
    } else {
        if (find_keyword("6:length", &i) == 1) {
            i += 8;
            i++;
            GET_NUM('e', file_length);
        }
    }
#ifdef DEBUG
    printf("File length:%lld\n", file_length);
#endif
    return 0;
}

int get_files_length_path()
{
    long   i;
    long   length;
    int    count;
    Files *node = NULL, *p = NULL;

    if (!is_multi_files()) return 0;
    for (i = 0; i < filesize - 8; i++) {
        /*improve*/
        if (memcmp(&metafile_content[i], "6:length", 8) == 0) {
            i += 8 + 1;  // 跳过int起始标记'i'
            //读取length
            length = 0;
            GET_NUM('e', length);
            //建立新节点
            node         = (Files *)malloc(sizeof(Files));
            node->length = length;
            node->next   = NULL;

            //插入列表
            if (files_head == NULL)
                files_head = node;
            else {
                p = files_head;
                while (p->next != NULL) p = p->next;
                p->next = node;
            }
        }
        if (memcmp(&metafile_content[i], "4:path", 6) == 0) {
            i += 6 + 1;  // 跳过list起始标记'l'
            //读取文件个数
            count = 0;
            GET_NUM(':', count);
            p = files_head;
            while (p->next) p = p->next;
            memcpy(p->path, &metafile_content[i], count);
            *(p->path + count) = '\0';
        }
    }

#ifdef DEBUG
    int t = 0;
    p     = files_head;
    while (p != NULL) {
        t++;
        printf("File %d. %20ld: %s\n", t, p->length, p->path);
        p = p->next;
    }
#endif
    return 0;
}

int get_info_hash()
{
    int  push_pop = 0;
    long i, begin = 0, end = 0;

    if (metafile_content == NULL) return -1;

    //查找"4:info"关键字
    if (find_keyword("4:info", &i) != 1) return -1;

    i += 6;
    begin = i;  //记录info字段起点

    while (i < filesize && !end) {
        switch (metafile_content[i]) {
            case 'd':  // dict
            case 'l':  // list
                push_pop++;
                i++;
                break;
            case 'i':  // 数字,向后直到找到'e'为止
                i++;
                if (i == filesize) return -1;
                //找'e'
                while (metafile_content[i] != 'e')
                    if (++i == filesize) return -1;
                //跳过数字结束标记'e'
                i++;
                break;
            case 'e':  // dict或list的结尾
                push_pop--;
                if (push_pop == 0)
                    end = i;  //记录info字段终点
                else
                    i++;
                break;
            default:
                if (metafile_content[i] >= '0' &&
                    metafile_content[i] <= '9') {  // digit
                    //跳过这个数字表示的距离
                    int number = 0;
                    GET_NUM(':', number);
                    i += number;
                    break;
                } else  // unknown character
                {
                    printf("%s:%d wrong, unknown character (%c) at %ld\n",
                        __FILE__, __LINE__, metafile_content[i], i);
                    return -1;
                }
        }
    }

    if (i == filesize) return -1;

    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, &metafile_content[begin], end - begin + 1);
    SHA1Final(info_hash, &context);
#ifdef DEBUG
    printf("info_hash:");
    for (int i = 0; i < 20; i++) printf("%.2x", info_hash[i]);
    printf("\n");
#endif
    return 0;
}
//生成一个独特的peer_id
int get_peer_id()
{
    srand(time(NULL));
    sprintf((char *)peer_id, "-TT1000-%012d", rand());
#ifdef DEBUG
    printf("peer_id:%s\n", peer_id);
#endif
    return 0;
}
//释放解析种子文件分配的内存
void release_memory_in_parse_metafile()
{
    Announce_list *p;
    Files *        q;
    if (metafile_content) free(metafile_content);
    if (file_name) free(file_name);
    if (pieces_hash) free(pieces_hash);
    /*free list*/
    while (announce_list_head) {
        p                  = announce_list_head;
        announce_list_head = announce_list_head->next;
        free(p);
    }
    while (files_head) {
        q          = files_head;
        files_head = files_head->next;
        free(q);
    }
}

int parse_metafile(char *metafile)
{
    if (read_metafile(metafile) < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (read_announce_list() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (is_multi_files() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_piece_length() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_pieces_hash() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_file_name() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_files_length_path() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_info_hash() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    if (get_peer_id() < 0) {
        printf("%s:%d wrong\n", __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

int get_files_count()
{
    if (file_count == -1) {
        if (is_multi_files() == 0)
            file_count = 1;
        else {
            Files *p   = files_head;
            file_count = 0;
            while (p != NULL) {
                file_count++;
                p = p->next;
            }
        }
    }
    return file_count;
}