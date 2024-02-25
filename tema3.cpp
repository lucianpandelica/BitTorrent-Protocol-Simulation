#include <mpi.h>
#include <pthread.h>
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

#include "client.h"
#include "tracker.h"

using namespace std;

/* stored files */
int num_stored_files;
StoredFile* client_stored;

/* wanted files - for tracker */
int num_wanted_files;
TrackerWantedFile* tracker_wanted_init;

/* wanted files - from tracker */
ClientData* client_data;

void init_tracker_stored_file(int size)
{
    num_stored_files = size;
    client_stored = (StoredFile*) malloc(size * sizeof(StoredFile));
}

void init_tracker_wanted_file(int size)
{
    num_wanted_files = size;
    tracker_wanted_init = (TrackerWantedFile*)
                          malloc(size * sizeof(TrackerWantedFile));
}

void init_client_data(int rank)
{
    int res;

    int num_tasks;
    res = MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    check_ret_code(res, "MPI_Comm_size()\n");
    client_data = new ClientData(num_tasks, rank);
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;

    init_client_data(rank);

    /* open file */
    std::string in_file;
    in_file = "in" + std::to_string(rank) + ".txt";
    ifstream f(in_file);

    /* read number of stored files */
    int my_files_num;
    f >> my_files_num;

    /* init vector of stored files */
    init_tracker_stored_file(my_files_num);

    /* read client's stored files */
    for (int i {0}; i < my_files_num; i++) {
        std::string name;
        int segm_num;

        f >> name >> segm_num;
        strcpy(client_stored[i].name, name.c_str());
        client_stored[i].segm_num = segm_num;

        for (int j {0}; j < segm_num; j++) {
            /* 32 bit strings */
            std::string hash;
            f >> hash;
            strcpy(client_stored[i].segm[j], hash.c_str());
        }

        /* points to the element representing current file */
        client_data->stored_files[name] = client_stored + i;
    }
    
    /* read number of wanted files */
    int wanted_files_num;
    f >> wanted_files_num;

    /* init vector of wanted files */
    init_tracker_wanted_file(wanted_files_num);
    client_data->init_files_wanted = wanted_files_num;

    /* read wanted files */
    for (int i {0}; i < wanted_files_num; i++) {
        std::string file_name;
        f >> file_name;
        strcpy(tracker_wanted_init[i].name, file_name.c_str());
    }

    /* close file */
    f.close();

    /* send number of files */
    send_int(num_stored_files, TRACKER_RANK, 0);

    /* send stored files hashes to the tracker */
    for (int i {0}; i < num_stored_files; i++) {
        /* send name of file */
        send_file_name(client_stored[i].name, TRACKER_RANK, 0);

        /* send number of segments */
        send_int(client_stored[i].segm_num, TRACKER_RANK, 0);

        /* send segments */
        for (int j {0}; j < client_stored[i].segm_num; j++)
            send_hash(client_stored[i].segm[j], TRACKER_RANK, 0);
    }

    /* wait for ACK */
    int response = recv_bcast(TRACKER_RANK);
    if (response != OK) {
        perror("ERROR: Tracker didn't send the right ACK.\n");
    }

    /* --- client can begin --- */

    /* send request to tracker */
    send_int(TYPE_REQ, TRACKER_RANK, 0);

    /* send number of wanted files */
    send_int(wanted_files_num, TRACKER_RANK, 0);

    /* send files names */
    for (int i {0}; i < wanted_files_num; i++)
        send_file_name(tracker_wanted_init[i].name, TRACKER_RANK, 0);

    /* receive response */
    for (int i {0}; i < wanted_files_num; i++) {
        /* file name */
        char crt_file_name[MAX_FILENAME + 1];
        recv_file_name(crt_file_name, TRACKER_RANK, 0);

        /* num of segm */
        int segm_num = recv_int(TRACKER_RANK, 0);

        /* add to client_data */
        std::string str_name(crt_file_name);
        WantedFile wf = WantedFile(segm_num);
        client_data->wanted_files[str_name] = wf;

        /* add to stored data - but only the name and num of segm
            (segments will be added when received) */
        client_data->addStoredFile(str_name, segm_num);

        /* segments */
        for (int j {0}; j < segm_num; j++) {
            char crt_segm_hash[HASH_SIZE + 1];
            recv_hash(crt_segm_hash, TRACKER_RANK, 0);

            /* add segment to file */
            client_data->wanted_files[str_name].addSegm(crt_segm_hash, j);

            int cli_num = recv_int(TRACKER_RANK, 0);

            for (int k {0}; k < cli_num; k++) {
                int crt_cli = recv_int(TRACKER_RANK, 0);
                /* add client to segment */
                client_data->wanted_files[str_name].addClient(j, crt_cli);
            }
        }
    }

    /* refresh updates before beginning to download */
    client_data->refresh_updates();

    int segment_counter = 0;

    /* main loop */
    while (true) {
        /* check if download is finished */
        if (client_data->files_finished == wanted_files_num) {
            break;
        }

        /* pick a wanted segment and ask a client to send it */
        client_data->request_segm();
        segment_counter++;

        /* every 10 segments received - send update to tracker */
        if (segment_counter == 10) {
            client_data->send_updates(rank);
            segment_counter = 0;
        }
    }

    /* send last updates */
    client_data->send_updates(rank);

    /* send message to tracker (all files downloaded) */
    client_data->send_download_finish();

    /* keep communication with tracker */
    while (true) {
        /* wait for tracker to tell that all clients finished */
        int code = recv_bcast(TRACKER_RANK);

        if (code == ALL_FINISHED) {
            /* when it arrives, shut down upload thread and break */
            client_data->shutdown_upload();
            break;
        }
    }

    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int res;

    /* wait for requests */
    while (true) {
        MPI_Status status;

        /* get file name from any client */
        char file_name[MAX_FILENAME + 1];
        res = MPI_Recv(file_name,
                       MAX_FILENAME + 1,
                       MPI_CHAR,
                       MPI_ANY_SOURCE,
                       UPLOAD_TAG,
                       MPI_COMM_WORLD,
                       &status);
        check_ret_code(res, "ERROR: MPI_Recv\n");
        int source_client = status.MPI_SOURCE;

        /* get segment index from the client that sent message */
        int segm_index = recv_int(source_client, UPLOAD_TAG);

        /* check for shutdown */
        if (segm_index == INVALID_SEGM)
            break;
        
        std::string str_file_name(file_name);

        /* send segment hash */
        send_hash(client_data->stored_files[str_file_name]->segm[segm_index],
                  source_client,
                  DOWNLOAD_TAG);
    }

    return NULL;
}

void tracker(int numtasks, int rank) {
    int res;
    int no_clients;

    /* get number of clients */
    res = MPI_Comm_size(MPI_COMM_WORLD, &no_clients);
    check_ret_code(res, "ERROR: MPI_Comm_size\n");
    /* substract us, the tracker */
    no_clients -= 1;

    /* receive data from every client and store it */
    TrackerData myTrackData = TrackerData(no_clients);

    for (int i {1}; i <= no_clients; i++) {
        int no_stored_files = recv_int(i, 0);

        for (int j {0}; j < no_stored_files; j++) {
            char name[MAX_FILENAME + 1];
            bool had_file = false;

            /* get file name */
            recv_file_name(name, i, 0);
            
            std::string str_name(name);
            if (myTrackData.files.count(str_name) != 0) {
                /* tracker already had this file */
                had_file = true;
            }

            int no_segm = recv_int(i, 0);

            std::vector<Segment> file_segm = std::vector<Segment>();

            for (int k {0}; k < no_segm; k++) {
                char segm[HASH_SIZE + 1];
                recv_hash(segm, i, 0);

                if (had_file == true) {
                    /* update clients */
                    myTrackData.files[str_name][k].clients.insert(i);
                } else {
                    /* create segment object and add to vector */
                    Segment new_segm = Segment(segm, k, i);
                    file_segm.push_back(new_segm);
                }
            }

            if (had_file == false) {
                /* add file to tracker data */
                myTrackData.addEntry(name, file_segm);
            }
        }

        /* finished taking data from this client */
    }

    /* send ACK to all clients */
    int code = OK;
    res = MPI_Bcast(&code, 1, MPI_INT, TRACKER_RANK, MPI_COMM_WORLD);

    /* loop - wait for messages from client*/
    while (true) {
        int type;
        MPI_Status status;
        
        res = MPI_Recv(&type,
                       1,
                       MPI_INT,
                       MPI_ANY_SOURCE,
                       0,
                       MPI_COMM_WORLD,
                       &status);
        check_ret_code(res, "ERROR: MPI_Recv\n");
        int source_client = status.MPI_SOURCE;

        if (type == TYPE_REQ) {
            /* receive wanted files number */
            int wanted_files_num = recv_int(source_client, 0);

            /* receive wanted files and store them */
            int wanted_files_known = 0;
            for (int i {0}; i < wanted_files_num; i++) {
                char file_name[MAX_FILENAME + 1];
                recv_file_name(file_name, source_client, 0);

                /* add file name to set in class */
                std::string str_file_name(file_name);
                myTrackData.wanted_files[source_client].insert(str_file_name);

                if (myTrackData.files.count(str_file_name) != 0)
                    wanted_files_known++;
            }

            if (wanted_files_known != wanted_files_num) {
                perror("ERROR: Some files don't exist in the network.\n");
            }

            /* send file data */
            for (auto elem : myTrackData.wanted_files[source_client]) {
                /* file name */
                send_file_name(elem, source_client, 0);

                /* number of segments */
                int num_segm = myTrackData.files[elem].size();
                send_int(num_segm, source_client, 0);

                /* send segments */
                for (int j {0}; j < num_segm; j++) {
                    /* segment hash */
                    std::string hash = myTrackData.files[elem][j].hash;
                    send_hash(hash, source_client, 0);

                    /* number of clients */
                    int clients_num = myTrackData.files[elem][j].clients.size();
                    send_int(clients_num, source_client, 0);

                    /* client IDs list */
                    for (int cli : myTrackData.files[elem][j].clients)
                        send_int(cli, source_client, 0);
                }
            }

        } else if (type == TYPE_UPD) {
            myTrackData.recv_update(source_client);
            myTrackData.send_update(source_client);
        } else if (type == TYPE_FILE_FIN) {
            myTrackData.recv_file_finish(source_client);
        } else if (type == TYPE_CLI_FIN) {
            /* don't need to receive anything */

            /* mark as finished */
            myTrackData.finished_cli++;

            /* send ACK */
            send_int(OK, source_client, 0);

            if (myTrackData.finished_cli == myTrackData.all_cli) {
                break;
            }
        }
    }

    /* all clients finished - tell every client to shutdown */
    code = ALL_FINISHED;
    res = MPI_Bcast(&code, 1, MPI_INT, TRACKER_RANK, MPI_COMM_WORLD);
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
    
    /* make sure we get true random numbers */
    srand((unsigned int)time(0));

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
