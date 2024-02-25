#include "client.h"

ClientData::ClientData(int total_clients_num, int rank) {
    updates = std::unordered_map<std::string, std::unordered_set<int>>();
    stored_files = std::unordered_map<std::string, StoredFile*>();
    wanted_files = std::unordered_map<std::string, WantedFile>();
    files_finished = 0;
    requests = std::vector<int>(total_clients_num, 0);
    ID = rank;
}

void ClientData::request_segm() {
    /* decide which file */
    int crt_wanted_files;
    crt_wanted_files = init_files_wanted - files_finished;

    int map_index = rand() % crt_wanted_files;
    if (map_index > (int) (wanted_files.size() - 1))
        perror("ERROR: Invalid file index.\n");

    std::unordered_map<std::string, WantedFile>::iterator it;
    it = wanted_files.begin();
    int steps = map_index;

    while (steps > 0) {
        it++;
        steps -= 1;
    }

    std::string file_name = (*it).first;

    /* decide segment index */
    std::vector<Segment> segments = (*it).second.segm;
    std::unordered_set<int> set = (*it).second.wanted_segm;
    int wanted_segm_num;
    wanted_segm_num = set.size();

    int set_index = rand() % wanted_segm_num;
    if (set_index > (int) (set.size() - 1))
        perror("ERROR: Invalid segment index.\n");

    std::unordered_set<int>::iterator it_set;
    it_set = set.begin();
    steps = set_index;

    while (steps > 0) {
        it_set++;
        steps -= 1;
    }

    int segm_index = (*it_set);

    /* decide destination client */
    int client_rank;
    int min_req = INT_MAX;

    for (int c : segments[segm_index].clients) {
        if (requests[c] < min_req) {
            min_req = requests[c];
            client_rank = c;
        }
    }

    /* --- send request to peer/seed --- */

    /* file name */
    send_file_name(file_name, client_rank, UPLOAD_TAG);
    /* segment index */
    send_int(segm_index, client_rank, UPLOAD_TAG);

    /* wait for segment hash */
    char segm_hash[HASH_SIZE + 1];
    recv_hash(segm_hash, client_rank, DOWNLOAD_TAG);

    /* verifiy hash */
    std::string recv_hash(segm_hash);
    std::string stored_hash = segments[segm_index].hash;

    if (recv_hash != stored_hash)
        perror("ERROR: Different hashes.\n");

    /* update data (remove index from set, increment number of recv segm etc.) */
    received_segm(file_name, segm_index, segm_hash, client_rank);
}

/* called after receiving a segment */
void ClientData::received_segm(std::string file,
                               int segm_index,
                               char hash[HASH_SIZE + 1],
                               int sender_rank) {
    /* store segment hash */
    strcpy(stored_files[file]->segm[segm_index], hash);
    /* update database */
    wanted_files[file].wanted_segm.erase(segm_index);
    wanted_files[file].recv_segm_num++;
    requests[sender_rank]++;
    insert_update(file, segm_index);
    
    /* finished receiving file */
    if ( wanted_files[file].recv_segm_num ==
            wanted_files[file].segm_num ) {
        wanted_files[file].finished = true;
        /* erase element */
        wanted_files.erase(file);
        /* inform tracker */
        send_file_finish(file);
        /* save file to client<R>_file<I>.txt */
        save_file(file);
        /* increment finished files */
        files_finished++;
    }
}

/* allocs stored file structure */
void ClientData::addStoredFile(std::string name, int segm_num) {
    StoredFile* new_file = (StoredFile*) malloc(sizeof(StoredFile));
    stored_files[name] = new_file;
    stored_files[name]->segm_num = segm_num;
}

void ClientData::shutdown_upload() {
    int res;

    /* send a filename - will not used */
    char placeholder[MAX_FILENAME + 1];
    res = MPI_Send(placeholder,
                   MAX_FILENAME + 1,
                   MPI_CHAR,
                   ID,
                   UPLOAD_TAG,
                   MPI_COMM_WORLD);
    check_ret_code(res, "ERROR: MPI_Send\n");

    /* send segment index -1 (to shutdown) */
    send_int(INVALID_SEGM, ID, UPLOAD_TAG);
}

void ClientData::send_download_finish() {
    /* message type */
    send_int(TYPE_CLI_FIN, TRACKER_RANK, 0);

    /* wait for ACK */
    int res = recv_int(TRACKER_RANK, 0);
    if (res != OK)
        perror("ERROR: Didn't receive the right ACK.\n");
}

void ClientData::send_file_finish(std::string file) {
    /* message type */
    send_int(TYPE_FILE_FIN, TRACKER_RANK, 0);

    /* file name */
    send_file_name(file, TRACKER_RANK, 0);
}

void ClientData::save_file(std::string file) {
    std::string dest_file = "client" + std::to_string(ID) + "_" + file;
    std::ofstream r(dest_file);

    for (int i {0}; i < stored_files[file]->segm_num; i++)
        r << stored_files[file]->segm[i] << "\n";

    r.close();
}

/* will be called after sending updates to tracker */
void ClientData::refresh_updates() {
    /* erase all elements */
    updates.clear();
}

/* will be called after receiving a new segment */
void ClientData::insert_update(std::string file, int segm_index) {
    if (updates.count(file) == 0)
        updates[file] = std::unordered_set<int>();
    
    updates[file].insert(segm_index);
}

/* sends all client updates to tracker */
void ClientData::send_updates(int ID) {
    /* message type */
    send_int(TYPE_UPD, TRACKER_RANK, 0);

    /* number of files updated */
    send_int(static_cast<int>(updates.size()), TRACKER_RANK, 0);

    for (auto elem : updates) {
        /* file name */
        send_file_name(elem.first, TRACKER_RANK, 0);

        /* number of segments */
        send_int(static_cast<int>(elem.second.size()), TRACKER_RANK, 0);

        /* segments */
        for (int segm_index : elem.second)
            send_int(segm_index, TRACKER_RANK, 0);
    }

    /* erase updates */
    refresh_updates();

    /* receives updates from tracker */
    int num_files_upd = recv_int(TRACKER_RANK, 0);

    for (int i {0}; i < num_files_upd; i++) {
        /* file name */
        char file_name[MAX_FILENAME + 1];
        recv_file_name(file_name, TRACKER_RANK, 0);
        std::string str_file_name(file_name);

        /* number of segments to update */
        int segm_num = recv_int(TRACKER_RANK, 0);

        for (int j {0}; j < segm_num; j++) {
            int index_segm = recv_int(TRACKER_RANK, 0);
            int new_cli_num = recv_int(TRACKER_RANK, 0);

            for (int k {0}; k < new_cli_num; k++) {
                int cli_id = recv_int(TRACKER_RANK, 0);

                /* update database */
                if (wanted_files.count(str_file_name) != 0) {
                    wanted_files[str_file_name].addClient(index_segm, cli_id);
                }
            }
        }
    }
}
