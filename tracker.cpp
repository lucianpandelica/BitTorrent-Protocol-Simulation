#include "tracker.h"

TrackerData::TrackerData(int cli_num) {
    files = std::unordered_map<std::string, std::vector<Segment>>();
    finished_cli = 0;
    all_cli = cli_num;
    
    /* init wanted files vector */
    wanted_files = std::vector<std::unordered_set<std::string>>(all_cli + 1);
    for (int i {0}; i <= all_cli; i++)
        wanted_files[i] = std::unordered_set<std::string>();

    /* init updates vector */
    updates = std::vector
              <std::unordered_map
              <std::string, std::unordered_map
                            <int, std::unordered_set<int>>>>(all_cli + 1);
    for (int i {0}; i <= all_cli; i++)
        updates[i] = std::unordered_map
                     <std::string, std::unordered_map
                                   <int, std::unordered_set<int>>>();
}

void TrackerData::addEntry(std::string name, std::vector<Segment> segm) {
    files[name] = segm;
}

void TrackerData::recv_file_finish(int source_client) {
    char file_name[MAX_FILENAME + 1];
    recv_file_name(file_name, source_client, 0);
    std::string str_file_name(file_name);

    /* update database */
    if (wanted_files[source_client].count(file_name) != 0)
        wanted_files[source_client].erase(file_name);
    else
        perror("Tracker can't erase wanted file from client (doesn't exist)\n");
}

void TrackerData::refresh_updates(int client_id) {
    /* erase updates for client */
    updates[client_id].clear();    
}

/* inserts update (file - segment index - new-client-id) for clients interested */
/* checks 'wanted_files' vector */
void TrackerData::insert_update(std::string file, int segm_index, int new_cli) {
    /* client IDs that want this file */
    std::unordered_set<int> want_file = std::unordered_set<int>();

    for (int i {1}; i <= all_cli; i++)
        if (wanted_files[i].count(file) != 0)
            want_file.insert(i);

    /* insert updates for these clients */
    for (int cli_id : want_file) {
        /* avoid sending back to client updates about itself */
        if (cli_id == new_cli)
            continue;

        /* insert file if it doesn't exist */
        if (updates[cli_id].count(file) == 0)
            updates[cli_id][file] = std::unordered_map
                                    <int, std::unordered_set<int>>();

        /* insert segment if it doesn't exist */
        if (updates[cli_id][file].count(segm_index) == 0)
            updates[cli_id][file][segm_index] = std::unordered_set<int>();

        updates[cli_id][file][segm_index].insert(new_cli);
    }
}

void TrackerData::send_update(int dest_client) {
    /* number of files */
    int num_files_upd = static_cast<int>(updates[dest_client].size());
    send_int(num_files_upd, dest_client, 0);

    /* updates for each file */
    for (auto file : updates[dest_client]) {
        /* file name */
        std::string file_name = file.first;
        send_file_name(file_name, dest_client, 0);

        /* number of segments */
        int num_segm = static_cast<int>(file.second.size());
        send_int(num_segm, dest_client, 0);

        /* segments */
        for (auto segm : file.second) {
            /* segm index */
            int index_segm = segm.first;
            send_int(index_segm, dest_client, 0);

            /* new clients number */
            int new_cli_num = static_cast<int>(segm.second.size());
            send_int(new_cli_num, dest_client, 0);

            /* client IDs */
            for (int cli_id : segm.second)
                send_int(cli_id, dest_client, 0);
        }
    }

    /* erase updates */
    refresh_updates(dest_client);
}

void TrackerData::recv_update(int source_client) {
    /* number of files updated */
    int num_files_upd;
    num_files_upd = recv_int(source_client, 0);

    /* files */
    for (int i {0}; i < num_files_upd; i++) {
        /* file name */
        char file_name[MAX_FILENAME + 1];
        recv_file_name(file_name, source_client, 0);
        std::string str_file_name(file_name);

        /* number of segments */
        int num_segm;
        num_segm = recv_int(source_client, 0);

        /* segments */
        for (int j {0}; j < num_segm; j++) {
            int segm_index;
            segm_index = recv_int(source_client, 0);

            /* insert update */
            insert_update(str_file_name, segm_index, source_client);
        }
    }
}
