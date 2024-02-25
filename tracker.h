#ifndef TRACKER_H
#define TRACKER_H

#include "helpers.h"

class TrackerData {
public:
    std::vector
    <std::unordered_map
    <std::string, std::unordered_map
                  <int, std::unordered_set<int>>>> updates;
    /* (file_name, file_segments) */
    std::unordered_map<std::string, std::vector<Segment>> files;
    std::vector<std::unordered_set<std::string>> wanted_files;
    int finished_cli;
    int all_cli; /* number of clients (excluding tracker) */

    TrackerData(int cli_num);

    void addEntry(std::string name, std::vector<Segment> segm);
    void recv_file_finish(int source_client);
    void refresh_updates(int client_id);
    void insert_update(std::string file, int segm_index, int new_cli);
    void send_update(int dest_client);
    void recv_update(int source_client);
};

#endif