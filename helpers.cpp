#include "helpers.h"

/* Segment class */
Segment::Segment() {
    clients = std::unordered_set<int>();
}

Segment::Segment(char* segm, int offset) {
    clients = std::unordered_set<int>();
    std::string str_segm(segm);
    this->hash = str_segm;
    this->offset = offset;
}

Segment::Segment(char* segm, int offset, int client_id)
    : Segment(segm, offset) {
    this->clients.insert(client_id);
}

void Segment::insCli(int cli_id) {
    this->clients.insert(cli_id);
}

/* WantedFile class */
WantedFile::WantedFile() {
    segm = std::vector<Segment>();
    wanted_segm = std::unordered_set<int>();
}

WantedFile::WantedFile(int segm_num)
    : WantedFile() {
    /* set all segments as wanted */
    for (int i {0}; i < segm_num; i++)
        this->wanted_segm.insert(i);

    this->segm_num = segm_num;
    this->recv_segm_num = 0;
    this->finished = false;
}

/* adds file segment hash */
void WantedFile::addSegm(char* hash, int offset) {
    Segment new_segm = Segment(hash, offset);
    this->segm.push_back(new_segm);
}

/* adds a client we can ask to send this segment */
void WantedFile::addClient(int segm_index, int client_id) {
    this->segm[segm_index].insCli(client_id);
}

/* other functions */

void c_from_string(char **dest, std::string source, int size)
{
    (*dest) = (char*) malloc(size);
    const char* c_source = source.c_str();
    strcpy((*dest), c_source);
}

void check_ret_code(int code, std::string message)
{
    if (code != MPI_SUCCESS)
        std::cout << message << "\n";
}

void send_file_name(char name[MAX_FILENAME + 1], int dest, int tag) {
    int res;

    res = MPI_Send(name, MAX_FILENAME + 1, MPI_CHAR, dest, tag, MPI_COMM_WORLD);
    check_ret_code(res, "ERROR: MPI_Send\n");
}

/* overload */
void send_file_name(std::string file_name, int dest, int tag) {
    char* c_file_name = NULL;
    c_from_string(&c_file_name, file_name, MAX_FILENAME + 1);

    send_file_name(c_file_name, dest, tag);

    free(c_file_name);
}

void send_hash(char hash[HASH_SIZE + 1], int dest, int tag) {
    int res;

    res = MPI_Send(hash, HASH_SIZE + 1, MPI_CHAR, dest, tag, MPI_COMM_WORLD);
    check_ret_code(res, "ERROR: MPI_Send\n");
}

/* overload */
void send_hash(std::string hash, int dest, int tag) {
    char* c_hash = NULL;
    c_from_string(&c_hash, hash, HASH_SIZE + 1);

    send_hash(c_hash, dest, tag);

    free(c_hash);
}

void send_int(int value, int dest, int tag) {
    int res;

    int to_send = value;
    res = MPI_Send(&to_send, 1, MPI_INT, dest, tag, MPI_COMM_WORLD);
    check_ret_code(res, "ERROR: MPI_Send\n");
}

int recv_int(int source, int tag) {
    int res;
    int value;

    res = MPI_Recv(&value,
                   1,
                   MPI_INT,
                   source,
                   tag,
                   MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
    check_ret_code(res, "ERROR: MPI_Recv\n");

    return value;
}

void recv_file_name(char file_name[MAX_FILENAME + 1], int source, int tag) {
    int res;

    res = MPI_Recv(file_name,
                   MAX_FILENAME + 1,
                   MPI_CHAR,
                   source,
                   tag,
                   MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
    check_ret_code(res, "ERROR: MPI_Recv\n");
}

void recv_hash(char hash[HASH_SIZE + 1], int source, int tag) {
    int res;

    res = MPI_Recv(hash,
                   HASH_SIZE + 1,
                   MPI_CHAR,
                   source,
                   tag,
                   MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE);
    check_ret_code(res, "ERROR: MPI_Recv\n");
}

int recv_bcast(int sender_rank) {
    int res;
    int value;

    res = MPI_Bcast(&value, 1, MPI_INT, sender_rank, MPI_COMM_WORLD);
    check_ret_code(res, "ERROR: MPI_Bcast\n");

    return value;
}
