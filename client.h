#ifndef CLIENT_H
#define CLIENT_H

#include "helpers.h"

class ClientData {
public:
    /* file_name, segment_indexes */
    std::unordered_map<std::string, std::unordered_set<int>> updates;
    /* file_name, file_data */
    std::unordered_map<std::string, StoredFile*> stored_files;
    /* file_name, file_data */
    std::unordered_map<std::string, WantedFile> wanted_files;
    /* how many requests we made for each client */
    std::vector<int> requests;
    int init_files_wanted;
    int files_finished;
    int ID;

    ClientData(int total_clients_num, int rank);

    void request_segm();
    /* called after receiving a segment */
    void received_segm(std::string file,
                       int segm_index,
                       char hash[HASH_SIZE + 1],
                       int sender_rank);
    /* allocs stored file structure */
    void addStoredFile(std::string name, int segm_num);
    void shutdown_upload();
    void send_download_finish();
    void send_file_finish(std::string file);
    void save_file(std::string file);
    /* will be called after sending updates to tracker */
    void refresh_updates();
    /* will be called after receiving a new segment */
    void insert_update(std::string file, int segm_index);
    /* sends all client updates to tracker */
    void send_updates(int ID);
};

#endif
