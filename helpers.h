#ifndef HELPERS_H
#define HELPERS_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <climits>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define TYPE_REQ 0
#define TYPE_UPD 1
#define TYPE_FILE_FIN 2
#define TYPE_CLI_FIN 3

#define UPLOAD_TAG 1
#define DOWNLOAD_TAG 2

#define ALL_FINISHED 22
#define INVALID_SEGM -1
#define OK 0

typedef struct {
    char name[MAX_FILENAME + 1];
} TrackerWantedFile;

/* used for sending stored files hashes to tracker at the beginning */
typedef struct {
    char name[MAX_FILENAME + 1];
    int segm_num;
    char segm[MAX_CHUNKS][HASH_SIZE + 1];
} StoredFile;

class Segment {
public:
    std::string hash;
    int offset;
    std::unordered_set<int> clients; /* IDs for clients that have the segment */

    Segment();
    Segment(char* segm, int offset);
    Segment(char* segm, int offset, int client_id);

    void insCli(int cli_id);
};

class WantedFile {
public:
    int segm_num; // total number of segments
    int recv_segm_num; // number of received segments
    std::vector<Segment> segm; // all segment hashes - in order
    std::unordered_set<int> wanted_segm; // wanted segm numbers
    bool finished;

    WantedFile();
    WantedFile(int segm_num);

    /* adds file segment hash */
    void addSegm(char* hash, int offset);
    /* adds a client we can ask to send this segment */
    void addClient(int segm_index, int client_id);
};

void c_from_string(char **dest, std::string source, int size);
void check_ret_code(int code, std::string message);
void send_file_name(char name[MAX_FILENAME + 1], int dest, int tag);
/* overload */
void send_file_name(std::string file_name, int dest, int tag);
void send_hash(char hash[HASH_SIZE + 1], int dest, int tag);
/* overload */
void send_hash(std::string hash, int dest, int tag);
void send_int(int value, int dest, int tag);
int recv_int(int source, int tag);
void recv_file_name(char file_name[MAX_FILENAME + 1], int source, int tag);
void recv_hash(char hash[HASH_SIZE + 1], int source, int tag);
int recv_bcast(int sender_rank);

#endif
