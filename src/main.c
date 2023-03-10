#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stddef.h>
#define __USE_XOPEN
#include <time.h>

#define MAX_NEIGHBOURS 1000
#define MAX_CLIENT_PROCESSES 1000
#define MAX_SERVER_PROCESSES 1000

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KWHT  "\x1B[37m"
#define KCYN  "\x1B[36m"
#define KMAG  "\x1B[35m"

struct RegisterMessage {
	int type;
	int client;
	int responsibleServer;
	char timestamp[20];
};

struct SyncMessage {
	int request_cnt;
	int hours;
};

struct SearchingClientTimestampMessage {
	int source;
	int client;
	char timestamp[20];
};

void exec_test_file(char* test_file_name, int num_of_servers, int world_size);
void init_array(int *array, int size);
void print_array(int *array, int size);
int element_exists(int *array, int size, int element);
int find_not_received_neighbour(int *neighbours, int *received_from_neighbours);
int find_my_next_server(int world_rank, int num_of_servers);
int* init_torus_neighbours(int world_rank, int servers_num);
int explore(int *unexplored, int world_rank, int parent, int leader, int exception);
void clear_char_array(char *array, int size);
int isClientExistAsRegistered(struct RegisterMessage *requests, int size, int client); 

enum message_type {CONNECT, REGISTER, START_LEADER_ELECTION_CLIENTS, START_LEADER_ELECTION_SERVERS, ELECT, LEADER_BATTLE, 
	NEW_NEIGHBOUR, ACK, LEADER_ELECTION_CLIENT_DONE, LEADER_ELECTION_SERVER_DONE, LEADER_ANNOUNCEMENT, LEADER_ANNOUNCEMENT_ACK,
	LEADER, CLIENT_LEADER, PARENT, ALREADY, SERVER_LEADER, SYNC, SYNC_ACK, OVERTIME, SEARCHING_CLIENT, CLIENT_FOUND, PRINT, PRINT_ACK, TERMINATE};

enum register_type {IN, OUT};
enum directions {LEFT, TOP, BOTTOM, RIGHT};

int main(int argc, char** argv) {	
	int neighbours[MAX_NEIGHBOURS], received_from_neighbours[MAX_NEIGHBOURS], neighbours_i = 0;
	int number_of_neighbours = 0, terminate = 0;

	if(argc != 3) {
		printf("[ERROR] Invalid number of arguments..\n");
		return -1;
	}

	int num_of_servers  = atoi(argv[1]) * atoi(argv[1]);
	int servers_num_arg = atoi(argv[1]);
	// Initialize the MPI environment
	MPI_Init(NULL, NULL);
	
	// Get the number of processes
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	// Get the rank of process
	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	MPI_File file;
	int rc = MPI_File_open(MPI_COMM_WORLD, "info.txt", MPI_MODE_CREATE|MPI_MODE_WRONLY|MPI_MODE_APPEND, MPI_INFO_NULL, &file);
	if(rc) {
       		printf( "Unable to open file\n" );
  	}

	int rec_data, rec_tag, dummy = 0, neighbour_rank, sent_elect_count = 0, received_elect_count = 0, senter_rank, i_received_from_neighbours = 0, print_acks_cnt=0, print_senter;
	int client_leader_rank = -1, server_leader_rank = -1, leader_announcement_received_acks = 0, leader_announcement_senter;

	struct RegisterMessage register_data;

	int my_next_server, request_cnt = 0;

	MPI_Status status;
	if(world_rank == 0) { //coordinator code
		exec_test_file(argv[2], num_of_servers, world_size);
		MPI_File_close(&file);
	}
	else if(world_rank > num_of_servers) { //clients code
		int blocklengths[4]   = {1, 1, 1, 20};
		MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_CHAR};
		MPI_Aint offsets[4];
		offsets[0] = offsetof(struct RegisterMessage, type);
		offsets[1] = offsetof(struct RegisterMessage, client);
		offsets[2] = offsetof(struct RegisterMessage, responsibleServer);
		offsets[3] = offsetof(struct RegisterMessage, timestamp);			
		MPI_Datatype struct_type;
		MPI_Type_create_struct(4, blocklengths, offsets, types, &struct_type);
		MPI_Type_commit(&struct_type);

		init_array(neighbours, MAX_NEIGHBOURS);
		init_array(received_from_neighbours, MAX_NEIGHBOURS);
		while(!terminate) {
			MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			rec_tag = status.MPI_TAG;
			senter_rank = status.MPI_SOURCE;
    			if (status.MPI_TAG == REGISTER) {
        			MPI_Recv(&register_data, 1, struct_type, MPI_ANY_SOURCE, REGISTER, MPI_COMM_WORLD, &status);
    			}
		        else if(status.MPI_TAG == ACK) {
				MPI_Recv(&register_data, 1, struct_type, MPI_ANY_SOURCE, ACK, MPI_COMM_WORLD, &status);
			}	
			else {
				MPI_Recv(&rec_data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			}
			switch(rec_tag) {
				case CONNECT:
					number_of_neighbours++;
					neighbour_rank = rec_data;
					neighbours[neighbours_i++] = neighbour_rank; //Update my neighbours
					MPI_Send(&world_rank, 1, MPI_INT, neighbour_rank, NEW_NEIGHBOUR, MPI_COMM_WORLD); //Notify my new neighbour
					break;

				case NEW_NEIGHBOUR:
					number_of_neighbours++;
					neighbour_rank = rec_data;
					neighbours[neighbours_i++] = neighbour_rank; //Update my neighbours
					MPI_Send(&dummy, 1, MPI_INT, status.MPI_SOURCE, ACK, MPI_COMM_WORLD);
					break;

				case ACK:
					printf("Rank %d received <ACK>(for register) from server %d\n", senter_rank);
					MPI_Send(&dummy, 1, MPI_INT, 0, ACK, MPI_COMM_WORLD);
					break;


				case START_LEADER_ELECTION_CLIENTS:
					//printf("Rank %d received message START_LEADER_ELECTION_CLIENTS from %d\n", world_rank, senter_rank);
					if(neighbours[1] == -1) { //im leaf
						MPI_Send(&world_rank, 1, MPI_INT, neighbours[0], ELECT, MPI_COMM_WORLD);
					}
					break;

				case ELECT:
					received_elect_count++;
					received_from_neighbours[i_received_from_neighbours++] = senter_rank;
					//printf("Process %d received <ELECT> | from Process %d | rec_elects: %d/%d\n", 
						//	world_rank, senter_rank, received_elect_count, number_of_neighbours);
					
					if(received_elect_count == number_of_neighbours && sent_elect_count == 0) {
						printf("%sProcess %d(client) is the leader+%s\n", KCYN, world_rank, KWHT);
						client_leader_rank = world_rank;
						MPI_Send(&world_rank, 1, MPI_INT, 0, LEADER_ELECTION_CLIENT_DONE, MPI_COMM_WORLD);
					}	
					if(received_elect_count == number_of_neighbours) {
						printf("Process %d | Im i the leader? lets check..\n", world_rank);
						MPI_Send(&world_rank, 1, MPI_INT, senter_rank, LEADER_BATTLE, MPI_COMM_WORLD);
					}
					else if(received_elect_count == number_of_neighbours-1) {
						//send message to the missing received neighbour
						int neighbour_to_send = find_not_received_neighbour(neighbours, received_from_neighbours);
						//printf("Process %d sends <ELECT> to Process %d\n", world_rank, neighbour_to_send);
						MPI_Send(&world_rank, 1, MPI_INT, neighbour_to_send, ELECT, MPI_COMM_WORLD);
						sent_elect_count++;
					}
					break;

				case LEADER_BATTLE:
					//printf("Process %d received <LEADER_BATTLE> from process %d\n", world_rank, senter_rank);
					if(world_rank > senter_rank) {
						client_leader_rank = world_rank;
						printf("%sProcess %d(client) is the leader_%s\n", KCYN, world_rank, KWHT);
						for(int i = 0; i < MAX_NEIGHBOURS; i++) {
							if(neighbours[i] == -1) break;
							MPI_Send(&world_rank, 1, MPI_INT, neighbours[i], LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
						}
					}
					break;
				
				case LEADER_ANNOUNCEMENT:
					printf("Rank %d received <LEADER_ANNOUNCEMENT> from %d, where leader=%d\n", world_rank, senter_rank, rec_data);
					leader_announcement_senter = senter_rank;
					client_leader_rank = rec_data;
					for(int i = 0; i < MAX_NEIGHBOURS; i++) {
						if(neighbours[i] == -1) break;
						if(neighbours[i] == senter_rank) continue;
						MPI_Send(&rec_data, 1, MPI_INT, neighbours[i], LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
					}
					if(neighbours[1] == -1 && world_rank != client_leader_rank) {
						MPI_Send(&rec_data, 1, MPI_INT, senter_rank, LEADER_ANNOUNCEMENT_ACK, MPI_COMM_WORLD);
					}
					break;

				case LEADER_ANNOUNCEMENT_ACK:
					printf("Rank %d received <LEADER_ANNOUNCE_ACK>(%d/%d) from %d, where leader=%d\n", world_rank, leader_announcement_received_acks+1, number_of_neighbours, senter_rank, rec_data);
					leader_announcement_received_acks++;
					if(world_rank == client_leader_rank) {
						if(leader_announcement_received_acks == number_of_neighbours) {
							MPI_Send(&rec_data, 1, MPI_INT, 0, LEADER_ELECTION_CLIENT_DONE, MPI_COMM_WORLD);
							printf("As leader %d i notified coordinator\n", rec_data);
						}
					}
					else {
						if(leader_announcement_received_acks == number_of_neighbours-1)
							MPI_Send(&rec_data, 1, MPI_INT, leader_announcement_senter, LEADER_ANNOUNCEMENT_ACK, MPI_COMM_WORLD);
					}
					break;

				case SERVER_LEADER:
					server_leader_rank = rec_data;
					printf("Rank %d received <SERVER_LEADER> which is %d\n", world_rank, server_leader_rank);
					break;

				case REGISTER:
//					printf("Rank %d received <REGISTER> of type %d and timestamp %s\n", world_rank, register_data.type, register_data.timestamp);
					if(senter_rank == 0) request_cnt += 1;
					if(world_rank == client_leader_rank) {
						printf("Leader %d received <REGISTER>!!\n", world_rank);
						MPI_Send(&register_data, 1, struct_type, server_leader_rank, REGISTER, MPI_COMM_WORLD);
					}
					else {

						for(int i = 0; i < MAX_NEIGHBOURS; i++) {
							if(neighbours[i] == senter_rank) continue;
							if(neighbours[i] == -1) break;
							MPI_Send(&register_data, 1, struct_type, neighbours[i], REGISTER, MPI_COMM_WORLD);
						}
					}
					break;

				case PRINT:
					printf("Rank %d received <PRINT>\n", world_rank);
					print_senter = senter_rank;
					char buf[100] = {0};
					sprintf(buf, "CLIENT %d PROCESSED %d REQUESTS\n", world_rank, request_cnt);
					MPI_File_write_shared(file, buf, 100, MPI_CHAR, MPI_STATUS_IGNORE);	
					if(neighbours_i == 1) {
						MPI_Send(&dummy, 1, MPI_INT, senter_rank, PRINT_ACK, MPI_COMM_WORLD);
					}
					else {
						for(int i = 0; i < MAX_NEIGHBOURS; i++) {
							if(neighbours[i] == -1) break;
							if(neighbours[i] == senter_rank) continue;
							MPI_Send(&dummy, 1, MPI_INT, neighbours[i], PRINT, MPI_COMM_WORLD);
						}
					}
					break;

				case PRINT_ACK:
					print_acks_cnt += 1;
					printf("Rank %d received ACK from neighbour %d, acks received %d/%d\n", world_rank, senter_rank, print_acks_cnt, number_of_neighbours);
					if(world_rank == client_leader_rank) {
						printf("Rank %d(client-leader) received ACK from neighbour %d\n", world_rank, senter_rank);
						if(print_acks_cnt == number_of_neighbours) {
							printf("Rank %d (client-leader) received %d <PRINT_ACKS>\n", world_rank, print_acks_cnt); 
							MPI_Send(&dummy, 1, MPI_INT, 0, PRINT_ACK, MPI_COMM_WORLD);		
						}
					}
					else {
						if(print_acks_cnt == number_of_neighbours-1) {
							printf("Rank %d will send PRINT_ACK to %d\n", world_rank, print_senter);
							MPI_Send(&dummy, 1, MPI_INT, print_senter, PRINT_ACK, MPI_COMM_WORLD);
						}
					}
					break;

				case TERMINATE:
					printf("Rank %d received <TERMINATE> from coordinator\n", world_rank);
					terminate = 1;
					break;


			}
		}
	}
	else { //servers code
	
		struct RegisterMessage requests[100];
		int request_i = 0, total_hours = 0, received_sync_acks = 0;
		int register_clients_cnt = 0, client_in_out_pairs_cnt = 0;	
		double work_hours = 0;

		int blocklengths[4]   = {1, 1, 1, 20};
		MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_CHAR};
		MPI_Aint offsets[4];
		offsets[0] = offsetof(struct RegisterMessage, type);
		offsets[1] = offsetof(struct RegisterMessage, client);
		offsets[2] = offsetof(struct RegisterMessage, responsibleServer);
		offsets[3] = offsetof(struct RegisterMessage, timestamp);			
		MPI_Datatype struct_type;
		MPI_Type_create_struct(4, blocklengths, offsets, types, &struct_type);
		MPI_Type_commit(&struct_type);
		
		int blocklens[2]   = {1, 1};
		MPI_Datatype sync_types[2] = {MPI_INT, MPI_INT};
		MPI_Aint sync_offsets[2];
		sync_offsets[0] = offsetof(struct RegisterMessage, type);
		sync_offsets[1] = offsetof(struct RegisterMessage, client);
		MPI_Datatype sync_struct_type;
		MPI_Type_create_struct(2, blocklens, sync_offsets, sync_types, &sync_struct_type);
		MPI_Type_commit(&sync_struct_type);

		int search_client_blocklens[3]   = {1, 1, 20};
		MPI_Datatype search_client_types[3] = {MPI_INT, MPI_INT, MPI_CHAR};
		MPI_Aint search_client_offsets[3];
		offsets[0] = offsetof(struct SearchingClientTimestampMessage , source);
		offsets[1] = offsetof(struct SearchingClientTimestampMessage , client);
		offsets[2] = offsetof(struct SearchingClientTimestampMessage , timestamp);			
		MPI_Datatype search_client_struct_type;
		MPI_Type_create_struct(3, search_client_blocklens, search_client_offsets, search_client_types, &search_client_struct_type);
		MPI_Type_commit(&search_client_struct_type);


		int children[4], my_next_server, *neighbours, parent = world_rank, children_i = 0, client_leader, leader = world_rank, total_req = 0, print_acks_cnt = 0;	
		init_array(children, 4);
		neighbours     = init_torus_neighbours(world_rank, servers_num_arg);
		my_next_server = find_my_next_server(world_rank, servers_num_arg);
		
		int *unexplored    = (int*)malloc(sizeof(int)*4);
		char overtime[10], timestamp[20];
		struct SyncMessage sync_data;
		struct SearchingClientTimestampMessage searching_client; 
		if(servers_num_arg == 2) {
			unexplored[LEFT]   = neighbours[RIGHT];
			unexplored[TOP]    = neighbours[TOP];
			unexplored[BOTTOM] = -1;
		        unexplored[RIGHT]  = -1;	
		}
		else {
			unexplored[LEFT]   = neighbours[LEFT];
			unexplored[TOP]    = neighbours[TOP];
			unexplored[BOTTOM] = neighbours[BOTTOM];
			unexplored[RIGHT]  = neighbours[RIGHT];
		}

		while(!terminate) {
			MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			rec_tag = status.MPI_TAG;
			senter_rank = status.MPI_SOURCE;
    			if (status.MPI_TAG == REGISTER) {
        			MPI_Recv(&register_data, 1, struct_type, MPI_ANY_SOURCE, REGISTER, MPI_COMM_WORLD, &status);
    			}
		        else if(status.MPI_TAG == SYNC_ACK) {
				MPI_Recv(&sync_data, 1, sync_struct_type, MPI_ANY_SOURCE, SYNC_ACK, MPI_COMM_WORLD, &status);
			}
			else if(status.MPI_TAG == SYNC) {
				MPI_Recv(&sync_data, 1, sync_struct_type, MPI_ANY_SOURCE, SYNC, MPI_COMM_WORLD, &status);
			}
			else if(status.MPI_TAG == OVERTIME) {
				MPI_Recv(overtime, 10, MPI_CHAR, 0, OVERTIME, MPI_COMM_WORLD, &status);
			}
			else if(status.MPI_TAG == CLIENT_FOUND) {
				MPI_Recv(&searching_client, 1, search_client_struct_type, MPI_ANY_SOURCE, CLIENT_FOUND, MPI_COMM_WORLD, &status);
			}
			else if(status.MPI_TAG == SEARCHING_CLIENT) {
				MPI_Recv(&searching_client, 1, search_client_struct_type, MPI_ANY_SOURCE, SEARCHING_CLIENT, MPI_COMM_WORLD, &status);
			}	
			else {
				MPI_Recv(&rec_data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			}

			switch(rec_tag) {
				
				case START_LEADER_ELECTION_SERVERS:
					printf("Process %d received <START_LEADER_ELECTION_SERVERS>\n", world_rank);
					print_array(unexplored, 4);
					if(explore(unexplored, world_rank, parent, leader, -2)) {
						printf("%sRank %d(server) is the Leader!%s\n", KCYN, world_rank, KWHT);
						MPI_Send(&leader, 1, MPI_INT, my_next_server, LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
					}
					break;

				case LEADER_ANNOUNCEMENT:
					printf("Rank %d received <LEADER_ANNOUNCEMENT> from %d\n", world_rank, senter_rank);
					if(world_rank == leader) {
						MPI_Send(&leader, 1, MPI_INT, 0, LEADER_ELECTION_SERVER_DONE, MPI_COMM_WORLD);
					}
					else {
						leader = rec_data;
						MPI_Send(&leader, 1, MPI_INT, my_next_server, LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
					}
					break;

				case CLIENT_LEADER:
					client_leader = rec_data;
					break;

				case LEADER:
					printf("Process %d received <LEADER> from %d\n", world_rank, senter_rank);
					if(leader<rec_data) {
						leader = rec_data;
						parent = senter_rank;
						init_array(children, 4);
						children_i = 0;
						for(int i = 0; i < 4; i++) {
							if(unexplored[i] == senter_rank)
								unexplored[i] = -1;
						}
						if(explore(unexplored, world_rank, parent, leader, senter_rank)) {
							MPI_Send(&leader, 1, MPI_INT, my_next_server, LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
							printf("%sRank %d(server) is the Leader!%s\n", KCYN, world_rank, KWHT);
						}
					}
					else if(leader == senter_rank) {
						MPI_Send(&leader, 1, MPI_INT, senter_rank, ALREADY, MPI_COMM_WORLD);
					}

					break;

				case ALREADY:
					printf("Process %d received <ALREADY> from %d\n", world_rank, senter_rank);
					if(rec_data == leader) {
						if(explore(unexplored, world_rank, parent, leader, -2)) {
							printf("%sRank %d(server) is the Leader!%s\n", KCYN, world_rank, KWHT);
							MPI_Send(&leader, 1, MPI_INT, my_next_server, LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
						}
					}
					break;

				case PARENT:
					//printf("Process %d received <PARENT> from %d\n", world_rank, senter_rank);
					if(rec_data == leader) {
						children[children_i++] = senter_rank;
						if(explore(unexplored, world_rank, parent, leader, -2)) {
							printf("%sRank %d is the Leader!%s\n", KCYN, world_rank, KWHT);
							MPI_Send(&leader, 1, MPI_INT, my_next_server, LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
						}
					}
					break;

				case REGISTER:
					printf("Server %d received <REGISTER> from %d\n", world_rank, senter_rank);
					if(world_rank == leader) {
						request_cnt++;
						printf("Leader Server %d requests received %d\n", world_rank, request_cnt);
						int s = request_cnt%num_of_servers+1;
						register_data.responsibleServer = s;
						if(s == leader) {
							if(isClientExistAsRegistered(requests, request_i, register_data.client) == 0) {
								printf("Rank %d client %d doesnt exists to any of my registers.Added!\n", world_rank, register_data.client);
								register_clients_cnt += 1;	
							}
							requests[request_i++] = register_data;
							MPI_Send(&register_data, 1, struct_type, requests[request_i-1].client, ACK, MPI_COMM_WORLD);
						}
						else MPI_Send(&register_data, 1, struct_type, my_next_server, REGISTER, MPI_COMM_WORLD);
					}		
					else {
						if(register_data.responsibleServer == world_rank) {
							//printf("Rank %d i will save the request!\n", world_rank);
							if(isClientExistAsRegistered(requests, request_i, register_data.client) == 0) {
								printf("Rank %d client %d doesnt exists to any of my registers.Added!\n", world_rank, register_data.client);
								register_clients_cnt += 1;	
							}
							requests[request_i++] = register_data;
							//printf("requested client %d\n", requests[request_i].client);
							MPI_Send(&register_data, 1, struct_type, requests[request_i].client, ACK, MPI_COMM_WORLD);
						}
						else {
							MPI_Send(&register_data, 1, struct_type, my_next_server, REGISTER, MPI_COMM_WORLD);
						}
					}

					break;

				case SYNC:
					if(world_rank == leader) {
						for(int i = 0; i < children_i; i++) {
							MPI_Send(&sync_data, 1, sync_struct_type, children[i], SYNC, MPI_COMM_WORLD);
						}
					}
					else {	
						struct SyncMessage message;
						struct tm tm1 = {0}, tm2 = {0};	
						time_t t1 = {0}, t2 = {0};
						int found = 0;
						double time;
						for(int i = 0; i < request_i; i++) {
							strptime(requests[i].timestamp, "%H:%M:%S %d/%m/%Y", &tm1);
							t1 = mktime(&tm1);
							printf("Rank %d has register of client %d type %d\n", world_rank, requests[i].client, requests[i].type);
							for(int j = 0; j < request_i; j++) {
								if(requests[j].client == requests[i].client && requests[j].type != requests[i].type) {
									found = 1;
									client_in_out_pairs_cnt += 1;
									strptime(requests[j].timestamp, "%H:%M:%S %d/%m/%Y", &tm2);
									t2 = mktime(&tm2);
									if(requests[j].type == OUT) {
										time = difftime(t2, t1)/3600;
										work_hours += time;
									}
									else {
										time = difftime(t1, t2)/3600;
										work_hours += time;
									}
									//printf("Rank %d - client %d | diff time = %lf\n", world_rank, requests[j].client, time);
									if(client_in_out_pairs_cnt == register_clients_cnt) {
										MPI_Send(&searching_client, 1, search_client_struct_type, leader, SYNC_ACK, MPI_COMM_WORLD);
									}
									break;
								}
							}
							if(found == 0) { //IN OR OUT of some client is not registered on my server
								int client = requests[i].client;
								searching_client.source = world_rank;
								searching_client.client = client;
								MPI_Send(&searching_client, 1, search_client_struct_type, my_next_server, SEARCHING_CLIENT, MPI_COMM_WORLD);
							}
						}
						//message.request_cnt = request_i;
						//message.hours 	    = work_hours;						
						//MPI_Send(&message, 1, sync_struct_type, senter_rank, SYNC_ACK, MPI_COMM_WORLD);
						for(int i = 0; i < children_i; i++) {
							if(children[children_i] == senter_rank) continue;
							MPI_Send(&sync_data, 1, sync_struct_type, children[i], SYNC, MPI_COMM_WORLD);
						}
					}
					break;

				case SEARCHING_CLIENT:
					printf("Rank %d received <SEARCHING_CLIENT> from %d\n", world_rank, senter_rank);
					int found = 0;

					for(int i = 0; i < request_i; i++) {
						if(requests[i].client == searching_client.client) {
							found = 1;
							strcpy(searching_client.timestamp, requests[i].timestamp);
							MPI_Send(&searching_client, 1, search_client_struct_type, my_next_server, CLIENT_FOUND, MPI_COMM_WORLD);
							break;
						}
					}
					if(found == 0) {
						MPI_Send(&searching_client, 1, search_client_struct_type, my_next_server, SEARCHING_CLIENT, MPI_COMM_WORLD);
					}
					break;

				case CLIENT_FOUND:
					printf("Rank %d received <CLIENT_FOUND> from %d\n", world_rank, senter_rank);
					if(world_rank == searching_client.source) {
						client_in_out_pairs_cnt += 1;
						printf("Rank %d | in_out_pairs = %d | total clients = %d\n", world_rank, client_in_out_pairs_cnt, register_clients_cnt);
						struct tm tm1 = {0}, tm2 = {0};	
						time_t t1 = {0}, t2 = {0};
						double time;
						strptime(searching_client.timestamp, "%H:%M:%S %d/%m/%Y", &tm1);
						t1 = mktime(&tm1);
						for(int i = 0; i < request_i; i++) {
							if(searching_client.client == requests[i].client) {
								printf("++++++++++++++++++++++++++++++++++++++++++\n");
								strptime(requests[i].timestamp, "%H:%M:%S %d/%m/%Y", &tm2);
								t2 = mktime(&tm2);
								if(requests[i].type == OUT) {
									time = difftime(t2, t1)/3600;
									work_hours += time;
								}
								else {
									time = difftime(t1, t2)/3600;
									work_hours += time;
								}
								break;
							}
						}
						printf("Rank %d | missing timestamp of client %d is %s\n", world_rank, searching_client.client, searching_client.timestamp);
						if(client_in_out_pairs_cnt == register_clients_cnt) { //all client hours are found/computed
							sync_data.request_cnt = request_i;
							sync_data.hours       = work_hours;
							printf("Rank %d TOTAL WORK HOURS %lf\n", world_rank, work_hours);
							MPI_Send(&sync_data, 1, sync_struct_type, parent, SYNC_ACK, MPI_COMM_WORLD);
						}
					}
					else {
						MPI_Send(&searching_client, 1, search_client_struct_type, my_next_server, CLIENT_FOUND, MPI_COMM_WORLD);
					}
					break;

				case SYNC_ACK:
					printf("Rank %d received <SYNC_ACK> from %d\n", world_rank, senter_rank);
					if(world_rank != leader) {
						MPI_Send(&sync_data, 1, sync_struct_type, parent, SYNC_ACK, MPI_COMM_WORLD);
					} else {
						received_sync_acks++;
						total_hours += sync_data.hours;
						total_req   += sync_data.request_cnt;
						if(world_rank == leader) {
							total_hours += sync_data.hours;
							total_req   += sync_data.request_cnt;
							if(received_sync_acks == num_of_servers-1) { 
								struct SyncMessage message;
								struct tm tm1 = {0}, tm2 = {0};	
								time_t t1 = {0}, t2 = {0};
								for(int i = 0; i < request_i; i++) {
									strptime(requests[i].timestamp, "%H:%M:%S %d/%m/%Y", &tm1);
									t1 = mktime(&tm1);
									for(int j = 0; j < request_i; j++) {
										if(requests[j].client == requests[i].client && requests[j].type != requests[i].type) {
											if(requests[j].type == OUT) {
												strptime(requests[j].timestamp, "%H:%M:%S %d/%m/%Y", &tm2);
												t2 = mktime(&tm2);
												double time = difftime(t2, t1)/3600;
												work_hours += time;
												//printf("Rank %d - client %d | diff time = %lf\n", world_rank, requests[j].client, time);
											}
											else {
												strptime(requests[j].timestamp, "%H:%M:%S %d/%m/%Y", &tm2);
												t2 = mktime(&tm2);
												double time = difftime(t1, t2)/3600;
												work_hours += time;
												//printf("Rank %d + client %d | diff time = %lf\n", world_rank, requests[j].client, time);
											}
											break;
										}
									}
								}
								total_hours += work_hours;
								total_req   += request_i;
								printf("%sServer Leader received %d <SYNC ACKS>. Including: %ld hours%s\n", KCYN, received_sync_acks, total_hours, KWHT);							
								MPI_Send(&dummy, 1, MPI_INT, 0, ACK, MPI_COMM_WORLD);
							}
						}
					}
					break;

				case OVERTIME:
					printf("Rank %d received <OVERTIME>\n", world_rank);
					break;

				case PRINT:
					printf("Rank %d received <PRINT> children = %d\n", world_rank, children_i);
					char buf[100] = {0};
					sprintf(buf, "SERVER %d HAS %d RECORDS with %ld HOURS\n", world_rank, request_i, work_hours);
					MPI_File_write_shared(file, buf, 100, MPI_CHAR, MPI_STATUS_IGNORE);	
					for(int i = 0; i < children_i; i++) {
						if(children[i] == -1) break;
						MPI_Send(&dummy, 1, MPI_INT, children[i], PRINT, MPI_COMM_WORLD);
					}
					MPI_Send(&dummy, 1, MPI_INT, senter_rank, PRINT_ACK, MPI_COMM_WORLD);
					break;

				case PRINT_ACK:
					if(world_rank == leader) {
						print_acks_cnt += 1;
						printf("print_acks_cnt = %d\n", print_acks_cnt);
						if(print_acks_cnt == num_of_servers-1) {
							printf("Rank %d (server-leader) received %d <PRINT_ACKS>\n", world_rank, print_acks_cnt); 
							MPI_Send(&dummy, 1, MPI_INT, client_leader, PRINT, MPI_COMM_WORLD);		
						}
					}
					else {
						MPI_Send(&dummy, 1, MPI_INT, parent, PRINT_ACK, MPI_COMM_WORLD);
					}
					break;

				case TERMINATE:
					printf("Rank %d received <TERMINATE> from coordinator\n", world_rank);
					terminate = 1;
					break;
			}
		}
	}

	// Finalize the MPI environment
	MPI_Finalize();
	exit(EXIT_SUCCESS);
}

int isClientExistAsRegistered(struct RegisterMessage *requests, int size, int client) {
	for(int i = 0; i < size; i++) {
		if(requests[i].client == client) return 1;	
	}
	return 0;
}

int explore(int *unexplored, int world_rank, int parent, int leader, int exception) {
	int process, is_empty = 1;
	for(int i = 0; i < 4; i++) {
		if(unexplored[i] != -1) {
			if(unexplored[i] == exception) continue;
			is_empty = 0;
			process = unexplored[i];
			unexplored[i] = -1;
			MPI_Send(&leader, 1, MPI_INT, process, LEADER, MPI_COMM_WORLD);	
			break;
		}
	}

	if(is_empty) {
		if(parent != world_rank) {
			MPI_Send(&leader, 1, MPI_INT, parent, PARENT, MPI_COMM_WORLD);
		}
		else {
			return 1;
		}
	}
	return 0;
}

void exec_test_file(char* test_file_name, int num_of_servers, int world_size) {
	FILE *fp;
	char file_data[100000], *line, *command;
	size_t line_len;
	
	fp = fopen(test_file_name, "r");
	if(!fp) {
		printf("[Error] fopen() failed!\n");
		return;
	}

	while(getline(&line, &line_len, fp) != -1) {
		strcat(file_data, line);
    	}

	int client_rank, neighbour_rank, dummy = 0, i, client_i = 0, server_i = 0, register_type;
	int client_processes[MAX_CLIENT_PROCESSES];
	int clients_leader_rank = -1, servers_leader_rank = 1;
	init_array(client_processes, MAX_CLIENT_PROCESSES);

	char *delims = " \n";
	char *token  = strtok(file_data, delims);

        while(token != NULL) {
	    	if(strcmp(token, "CONNECT") == 0) {
		    	client_rank    = atoi(strtok(NULL, delims));
		   	neighbour_rank = atoi(strtok(NULL, delims));
			if(!element_exists(client_processes, MAX_CLIENT_PROCESSES, client_rank)) client_processes[client_i++]    = client_rank;
			if(!element_exists(client_processes, MAX_CLIENT_PROCESSES, neighbour_rank)) client_processes[client_i++] = neighbour_rank;
			MPI_Send(&neighbour_rank, 1, MPI_INT, client_rank, CONNECT, MPI_COMM_WORLD);
			MPI_Recv(&dummy, 1, MPI_INT, client_rank, ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		else if(strcmp(token, "START_LEADER_ELECTION_SERVERS")==0) {
			for(i = 1; i <= num_of_servers; i++) {
				MPI_Send(&dummy, 1, MPI_INT, i, START_LEADER_ELECTION_SERVERS, MPI_COMM_WORLD);
			}
			MPI_Recv(&servers_leader_rank, 1, MPI_INT, MPI_ANY_SOURCE, LEADER_ELECTION_SERVER_DONE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("Coordinator received <LEADER_ELECTION_SERVER_DONE> | server leader %d\n", servers_leader_rank);
			for(i = num_of_servers+1; i < world_size; i++) {
				MPI_Send(&servers_leader_rank, 1, MPI_INT, i, SERVER_LEADER, MPI_COMM_WORLD);
			}
		}
		else if(strcmp(token, "START_LEADER_ELECTION_CLIENTS")==0) {
			for(i = num_of_servers+1; i < world_size; i++) {
				MPI_Send(&dummy, 1, MPI_INT, i, START_LEADER_ELECTION_CLIENTS, MPI_COMM_WORLD);
			}
			MPI_Recv(&clients_leader_rank, 1, MPI_INT, MPI_ANY_SOURCE, LEADER_ELECTION_CLIENT_DONE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("Coordinator received <LEADER_ELECTION_CLIENT_DONE> | client leader %d\n", clients_leader_rank);
			for(int i = 1; i <= num_of_servers; i++) {
				MPI_Send(&clients_leader_rank, 1, MPI_INT, i, CLIENT_LEADER, MPI_COMM_WORLD);
			}
		}
		else if(strcmp(token, "REGISTER")==0) {
			client_rank = atoi(strtok(NULL, delims));
			if(strcmp(strtok(NULL, delims), "IN") == 0) {
				register_type = IN;
			}
			else {
				register_type = OUT;
			}

			char time[9], date[11], time_date[20];
			
			clear_char_array(time, 9);
			clear_char_array(date, 11);
			clear_char_array(time_date, 20);

			strcpy(time, strtok(NULL, delims));
			strcpy(date, strtok(NULL, delims));

			strcat(time_date, time); strcat(time_date, " "); strcat(time_date, date);

			struct RegisterMessage request;
			
			int blocklengths[4]   = {1, 1, 1, 20};
			MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_CHAR};
			MPI_Aint offsets[4];
			offsets[0] = offsetof(struct RegisterMessage, type);
			offsets[1] = offsetof(struct RegisterMessage, client);
			offsets[2] = offsetof(struct RegisterMessage, responsibleServer);
			offsets[3] = offsetof(struct RegisterMessage, timestamp);			
			MPI_Datatype struct_type;
			MPI_Type_create_struct(4, blocklengths, offsets, types, &struct_type);
			MPI_Type_commit(&struct_type);

			request.type   = register_type;
			request.client = client_rank;
			request.responsibleServer = -1;
			strcpy(request.timestamp, time_date);
			MPI_Send(&request, 1, struct_type, client_rank, REGISTER, MPI_COMM_WORLD);

			MPI_Recv(&request, 1, struct_type, MPI_ANY_SOURCE, ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("CLIENT %d REGISTERED %d %s\n", request.client, request.type, request.timestamp);
		}
		else if(strcmp(token, "SYNC")==0) {
			printf("==START SYNC EVENT==\n");
			MPI_Send(&dummy, 1, MPI_INT, servers_leader_rank, SYNC, MPI_COMM_WORLD);
			MPI_Recv(&dummy, 1, MPI_INT, servers_leader_rank, ACK , MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("%sLEADER SYNC OK%s\n", KGRN, KWHT);	
		}
		else if(strcmp(token, "_OVERTIME")==0){
			printf("==OVERTIME EVENT STARTED==\n");
			int server_rank = atoi(strtok(NULL, delims));
			char date[10];
			strcpy(date, strtok(NULL, delims));
			MPI_Send(date, 10, MPI_CHAR, server_rank, OVERTIME, MPI_COMM_WORLD);
		}
		else if(strcmp(token, "PRINT")==0) {
			MPI_Send(&dummy, 1, MPI_INT, servers_leader_rank, PRINT, MPI_COMM_WORLD);
			MPI_Recv(&dummy, 1, MPI_INT, clients_leader_rank, PRINT_ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("Coordinator received <ACK>(PRINT) from client leader\n"); 
		}
            	token = strtok(NULL, delims);
        }
	printf("======= FILE EXECUTION COMPLETED ========\n");
	fclose(fp);
	//send termination message to other processes
	for(int i = 1; i < world_size; i++) MPI_Send(&dummy, 1, MPI_INT, i, TERMINATE, MPI_COMM_WORLD);
}

void clear_char_array(char *array, int size) {
	for(int i = 0; i < size; i++) array[i] = '\0';
}

int* init_torus_neighbours(int my_rank, int servers_num) {
	int* neighbours = (int*)malloc(sizeof(int)*4);
	assert(neighbours != NULL);
	
	if(my_rank <= servers_num) { //first row
		neighbours[TOP]   = servers_num*servers_num-servers_num+my_rank;
		neighbours[BOTTOM] = my_rank+servers_num;
		if(my_rank == servers_num) {
			neighbours[RIGHT] = 1;
			neighbours[LEFT]  = my_rank-1;
		}
		else if(my_rank == 1) {
			neighbours[RIGHT] = 2;
			neighbours[LEFT]  = servers_num;
		}
		else {
			neighbours[RIGHT] = my_rank + 1;
			neighbours[LEFT]  = my_rank - 1;
		}
	}
	else if(my_rank < servers_num*servers_num-servers_num) { //every row except first and last
		if(my_rank%servers_num == 1) { //first column
			neighbours[LEFT]   = my_rank + servers_num - 1;
			neighbours[TOP]    = my_rank - servers_num;
			neighbours[BOTTOM] = my_rank + servers_num;
			neighbours[RIGHT]  = my_rank + 1;
		}
		else if(my_rank%servers_num == 0) { //last column
			neighbours[LEFT]   = my_rank - 1;
			neighbours[TOP]    = my_rank - servers_num;
			neighbours[BOTTOM] = my_rank + servers_num;
			neighbours[RIGHT]  = my_rank - servers_num + 1;
		} else {
			neighbours[LEFT]   = my_rank-1;
			neighbours[TOP]    = my_rank - servers_num;
			neighbours[BOTTOM] = my_rank + servers_num;
			neighbours[RIGHT]  = my_rank+1;
		}
	}
	else { //last row
		neighbours[TOP] = my_rank - servers_num;
		neighbours[BOTTOM] = servers_num-(servers_num*servers_num-my_rank);
		if(my_rank > servers_num*servers_num-servers_num+1  && my_rank < servers_num*servers_num) {
			neighbours[RIGHT] = my_rank+1;
			neighbours[LEFT]  = my_rank-1;
		}
		else if(my_rank == servers_num*servers_num){ //rightest
			neighbours[RIGHT] = servers_num*servers_num-servers_num+1;
			neighbours[LEFT]  = my_rank-1;
		}
		else {
			neighbours[RIGHT] = my_rank+1;
			neighbours[LEFT]  = servers_num*servers_num;
		}
	}
	return neighbours;
}

int find_not_received_neighbour(int *neighbours, int *rec_neighbours){
	for(int i = 0; i < MAX_NEIGHBOURS; i++) {
		if(neighbours[i] == -1) break;
		for(int j = 0; j < MAX_NEIGHBOURS; j++) {
			if(neighbours[i] == rec_neighbours[j]) break;
			if(rec_neighbours[j] == -1) {
				return neighbours[i];
			}
		}
	}

	return -1;
}

void init_array(int *array, int size) {
	for(int i = 0; i < size; i++) array[i] = -1;
}

int element_exists(int *array, int size, int element) {
	for(int i = 0; i < size; i++) {
		if(array[i] == element) return 1;
	}
	return 0;
}

void print_array(int *array, int size) {
	printf("[");
	for(int i = 0; i < size; i++) { 
		if(array[i] == -1) {
			printf("]\n");
			return;
		}
		printf("%d,", array[i]);
	}
}

int find_my_next_server(int my_rank, int num_of_servers) {
	int my_next = -1;
	int row = ceil((double)my_rank/(double)num_of_servers);
	
	if(my_rank == num_of_servers * num_of_servers) return my_rank - num_of_servers+1; //rightest last row
	if(my_rank == num_of_servers * num_of_servers - num_of_servers + 1) return 1; // leftist last row

	if(row % 2 == 1) { //odd row
		if(my_rank % num_of_servers == 0) { //rightest
			my_next = my_rank + num_of_servers;
		}
		else { //not the rightest
			my_next = my_rank + 1;
		}
	}
	else { //even row 
		if((my_rank-1)%num_of_servers == 0) { //leftist and last
			my_next = my_rank + num_of_servers;
		}
		else {
			my_next = my_rank -1;
		}
	}
	return my_next;
}
