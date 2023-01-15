#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NEIGHBOURS 10

void exec_test_file(char* test_file_name);
void init_neighbours(int *neighbours);
void print_neighbours(int *neighbours);

enum message_type {CONNECT, REGISTER, NEW_NEIGHBOUR, ACK};

int main(int argc, char** argv) {
	
	int neighbours[MAX_NEIGHBOURS], neighbours_i = 0;

	if(argc != 3) {
		printf("[ERROR] Invalid number of arguments..\n");
		return -1;
	}

	// Initialize the MPI environment
	MPI_Init(NULL, NULL);
	
	// Get the number of processes
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
	// Get the rank of process
	int world_rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	int rec_data, rec_tag, dummy = 0, neighbour_rank;
	MPI_Status status;
	if(world_rank == 0) {
		exec_test_file(argv[2]);
		while(1){}
	}
	else {
		init_neighbours(neighbours);
		while(1) {
			MPI_Recv(&rec_data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			rec_tag = status.MPI_TAG;
			switch(rec_tag) {
				case CONNECT:
					neighbour_rank = rec_data;
					neighbours[neighbours_i++] = neighbour_rank; //Update my neighbours
					MPI_Send(&world_rank, 1, MPI_INT, neighbour_rank, NEW_NEIGHBOUR, MPI_COMM_WORLD); //Notify my new neighbour
					break;

				case NEW_NEIGHBOUR:
					neighbour_rank = rec_data;
					neighbours[neighbours_i++] = neighbour_rank; //Update my neighbours
					MPI_Send(&dummy, 1, MPI_INT, status.MPI_SOURCE, ACK, MPI_COMM_WORLD);
					break;

				case ACK:
					MPI_Send(&dummy, 1, MPI_INT, 0, ACK, MPI_COMM_WORLD);
					break;

				case REGISTER:
					break;
			}
		}
	}

	// Finalize the MPI environment
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

	int client_rank, neighbour_rank, dummy = 0;
	char *delims = " \n";
	char *token = strtok(file_data, delims);
        while(token != NULL) {
	    	if(strcmp(token, "CONNECT") == 0) {
		    	client_rank    = atoi(strtok(NULL, delims));
		   	neighbour_rank = atoi(strtok(NULL, delims));
			MPI_Send(&neighbour_rank, 1, MPI_INT, client_rank, CONNECT, MPI_COMM_WORLD);
			MPI_Recv(&dummy, 1, MPI_INT, client_rank, ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
		else if(strcmp(token, "REGISTER")==0) {
			MPI_Send(&dummy, 1, MPI_INT, 1, REGISTER, MPI_COMM_WORLD); 
		}
            	token = strtok(NULL, delims);
        }

	fclose(fp);
}

void init_neighbours(int *neighbours) {
	for(int i = 0; i < MAX_NEIGHBOURS; i++) neighbours[i] = -1;
}

void print_neighbours(int *neighbours) {
	printf("[");
	for(int i = 0; i < MAX_NEIGHBOURS; i++) printf("%d,",neighbours[i]);
	printf("]\n");
}


