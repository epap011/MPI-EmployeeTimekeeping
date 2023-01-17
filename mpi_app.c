#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NEIGHBOURS 1000
#define MAX_CLIENT_PROCESSES 1000
#define MAX_SERVER_PROCESSES 1000

void exec_test_file(char* test_file_name);
void init_array(int *array, int size);
void print_array(int *array, int size);
int element_exists(int *array, int size, int element);
int find_not_received_neighbour(int *neighbours, int *received_from_neighbours);

enum message_type {CONNECT, REGISTER, START_LEADER_ELECTION_CLIENTS, ELECT, LEADER_BATTLE, NEW_NEIGHBOUR, ACK
		, LEADER_ELECTION_CLIENTS_DONE, LEADER_ANNOUNCEMENT, LEADER_ANNOUNCEMENT_ACK};

int main(int argc, char** argv) {
	
	int neighbours[MAX_NEIGHBOURS], received_from_neighbours[MAX_NEIGHBOURS], neighbours_i = 0;
	int number_of_neighbours = 0;

	if(argc != 3) {
		printf("[ERROR] Invalid number of arguments..\n");
		return -1;
	}

	int num_of_servers = atoi(argv[1]) * atoi(argv[1]);

	// Initialize the MPI environment
	MPI_Init(NULL, NULL);
	
	// Get the number of processes
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	// Get the rank of process
	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	int rec_data, rec_tag, dummy = 0, neighbour_rank, sent_elect_count = 0, received_elect_count = 0, senter_rank, i_received_from_neighbours = 0;
	int leader_rank = 0, leader_announcement_received_acks = 0, leader_announcement_senter;
	MPI_Status status;
	if(world_rank == 0) { //coordinator code
		exec_test_file(argv[2]);
		while(1){}
	}
	else if(world_rank > num_of_servers) { //clients code
		init_array(neighbours, MAX_NEIGHBOURS);
		init_array(received_from_neighbours, MAX_NEIGHBOURS);
		while(1) {
			MPI_Recv(&rec_data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			rec_tag     = status.MPI_TAG;
			senter_rank = status.MPI_SOURCE;
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
					MPI_Send(&dummy, 1, MPI_INT, 0, ACK, MPI_COMM_WORLD);
					break;

				case REGISTER:
					break;

				case START_LEADER_ELECTION_CLIENTS:
					printf("Rank %d received message START_LEADER_ELECTION_CLIENTS from %d\n", world_rank, senter_rank);
					if(neighbours[1] == -1) { //im leaf
						MPI_Send(&world_rank, 1, MPI_INT, neighbours[0], ELECT, MPI_COMM_WORLD);
					}
					break;

				case ELECT:
					received_elect_count++;
					received_from_neighbours[i_received_from_neighbours++] = senter_rank;
					printf("Process %d received <ELECT> | from Process %d | rec_elects: %d/%d\n", 
							world_rank, senter_rank, received_elect_count, number_of_neighbours);
					
					if(received_elect_count == number_of_neighbours && sent_elect_count == 0) {
						printf("Process %d IM THE LEADER MUHAHAHHAHAHAHA\n", world_rank);
						MPI_Send(&world_rank, 1, MPI_INT, 0, LEADER_ELECTION_CLIENTS_DONE, MPI_COMM_WORLD);
					}	
					if(received_elect_count == number_of_neighbours) {
						printf("Process %d | Im i the leader? lets check..\n", world_rank);
						MPI_Send(&world_rank, 1, MPI_INT, senter_rank, LEADER_BATTLE, MPI_COMM_WORLD);
					}
					else if(received_elect_count == number_of_neighbours-1) {
						//send message to the missing received neighbour
						int neighbour_to_send = find_not_received_neighbour(neighbours, received_from_neighbours);
						printf("Process %d sends <ELECT> to Process %d\n", world_rank, neighbour_to_send);
						MPI_Send(&world_rank, 1, MPI_INT, neighbour_to_send, ELECT, MPI_COMM_WORLD);
						sent_elect_count++;
					}
					break;

				case LEADER_BATTLE:
					printf("Process %d received <LEADER_BATTLE> from process %d\n", world_rank, senter_rank);
					if(world_rank > senter_rank) {
						printf("Process %d IM THE LEADER MUHAHAH\n", world_rank);
						for(int i = 0; i < MAX_NEIGHBOURS; i++) {
							if(neighbours[i] == -1) break;
							MPI_Send(&world_rank, 1, MPI_INT, neighbours[i], LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
						}
					}
					break;
				
				case LEADER_ANNOUNCEMENT:
					leader_announcement_senter = senter_rank;
					for(int i = 0; i < MAX_NEIGHBOURS; i++) {
						if(neighbours[i] == -1) break;
						MPI_Send(&rec_data, 1, MPI_INT, neighbours[i], LEADER_ANNOUNCEMENT, MPI_COMM_WORLD);
					}
					//send this message to my neighbours except the one that i received

				case LEADER_ANNOUNCEMENT_ACK:
					leader_announcement_received_acks++;
					if(leader_announcement_received_acks == number_of_neighbours) {
						if(world_rank == rec_data) {
							MPI_Send(&world_rank, 1, MPI_INT, 0, LEADER_ELECTION_CLIENTS_DONE, MPI_COMM_WORLD);
						}
						else {
							MPI_Send(&world_rank, 1, MPI_INT, senter_rank, LEADER_ANNOUNCEMENT_ACK, MPI_COMM_WORLD);
						}
					}
					break;
			}
		}
	}
	else { //servers code

	}

	// Finalize the MPI environmen
	MPI_Finalize();
}

void exec_test_file(char* test_file_name) {
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

	int client_rank, neighbour_rank, dummy = 0, i, client_i = 0, server_i = 0;
	int client_processes[MAX_CLIENT_PROCESSES], server_processes[MAX_SERVER_PROCESSES];
	int clients_leader_rank = -1;
	init_array(client_processes, MAX_CLIENT_PROCESSES);
	init_array(server_processes, MAX_SERVER_PROCESSES);
	
	char *delims = " \n";
	char *token = strtok(file_data, delims);
        while(token != NULL) {
	    	if(strcmp(token, "CONNECT") == 0) {
		    	client_rank    = atoi(strtok(NULL, delims));
		   	neighbour_rank = atoi(strtok(NULL, delims));
			if(!element_exists(client_processes, MAX_CLIENT_PROCESSES, client_rank)) client_processes[client_i++]    = client_rank;
			if(!element_exists(client_processes, MAX_CLIENT_PROCESSES, neighbour_rank)) client_processes[client_i++] = neighbour_rank;
			MPI_Send(&neighbour_rank, 1, MPI_INT, client_rank, CONNECT, MPI_COMM_WORLD);
			MPI_Recv(&dummy, 1, MPI_INT, client_rank, ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		else if(strcmp(token, "START_LEADER_ELECTION_CLIENTS")==0) {
			for(i = 0; i < MAX_CLIENT_PROCESSES; i++) {
				if(client_processes[i] == -1) break;
				MPI_Send(&dummy, 1, MPI_INT, client_processes[i], START_LEADER_ELECTION_CLIENTS, MPI_COMM_WORLD);
			}
			MPI_Recv(&clients_leader_rank, 1, MPI_INT, MPI_ANY_SOURCE, LEADER_ELECTION_CLIENTS_DONE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			printf("Coordinator received <LEADER_ELECTION_CLIENTS_DONE | client leader %d\n", clients_leader_rank);
		}
		else if(strcmp(token, "REGISTER")==0) {
			MPI_Send(&dummy, 1, MPI_INT, 1, REGISTER, MPI_COMM_WORLD);
		}
            	token = strtok(NULL, delims);
        }

	fclose(fp);
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


