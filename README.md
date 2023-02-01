# MPI EmployeeTimekeeping - DistributedSystem
a distributed system employee timekeeping, which will manage employee entry and exit events from the workplace.
The project is implemented in C language using of the Message Passing Interface (MPI).

Implemented Events:

**CONNECT <client_rank> <neighbor_rank>** 
Upon this event, the coordinator initially sends a message of type CONNECT to the process with ID <client_rank> to update it
that its neighbor node in the tree is the process with identifier <neighbor_rank>. 
The process with id <client_rank> sends to process with id
<neighbor_rank> a message to inform it that it is a neighbor node. The process
with identifier <neighbor_rank> records it as one of its neighbor nodes and
sends an ACK back. When the <client_rank> process receives the ACK
in turn sends an ACK message to the coordinator to inform it of the termination
of the execution of this event.

**<START_LEADER_ELECTION_SERVERS>**
The coordinator sends a message of type <START_LEADER_ELECTION> to every server process present in the system and beyond
waiting to receive a message of type <LEADER_ELECTION_SERVERS_DONE> from the process
who will be elected leader.
When this message is received, the coordinator process sends a message of type
<SERVER_LEADER> containing the ID of the server-leader in all
processes clients, informing them of their role as clients, as well as the
ID of the elected leader server.

![Alt text](assets/images/tree.png?raw=true "Figure 1: an examples of client processes tree")

**<START_LEADER_ELECTION_CLIENTS>**
The coordinator sends a message of type <START_LEADER_ELECTION_CLIENT> in each client process present in the system and
then waits to receive a message of type <LEADER_ELECTION_CLIENTS_DONE> from
process to elect a leader.
When this message is received, the coordinator process sends a message of type
<CLIENT_LEADER> containing the ID of the client leader in all processes
servers, informing them of the ID of the elected leader client.

![Alt text](assets/images/torus.png?raw=true "Figure 1: an examples of server processes torus")

**REGISTER <client_rank> <TYPE> <TIMESTAMP>** 
Coordinator sends a register request to the client with identifier <client_rank> of type <TYPE> with date and
entry time specified by <TIMESTAMP>. The event type can have IN or OUT values depending on whether the employee enters or exits the company.
The TIMESTAMP it can be a string of the format “dd/MM/yyyy”.
When the client receives such a request, it forwards it to the leader client process,
through the customer skeleton tree. The customer must keep a counter with the amount
of the requests it has received. The client-master process sends the message to its master
servers and he decides which server, s, to forward it to
process. This is done as follows. The master server maintains a counter,
request_cnt, which counts how many REGISTER type requests it has received from clients each
time. The server will send the request to the node with an identifier
(s = request_cnt % NUM_SERVERS2 + 1). The dispatch will be done by forwarding the request to the next one
of the torus, which will continue forwarding to its next until the request arrives,
through the torus, at the node with identifier s.
When server s receives the request, it stores the entry and sends back one
<ACK> to the client. The client in turn should send an <ACK> type message
to the moderator and print:
CLIENT <client_rank> REGISTERED <TYPE> <TIMESTAMP>
  
**SYNC**
The coordinator sends a SYNC message to the leader server. The leader must
to get information about all the events stored in the remaining processes
servers and store them. Any server that receives the
message must send back a SYNC_ACK message containing two counters. The first
stores the number of requests stored on this server and o
second the total number of hours corresponding to the work done (calculating and
adding differences between IN and OUT events for the same ID), such as
recorded in cached requests on the server. When the leader receives the message
SYNC_ACK must check the entries it has received and calculate the grand total,
adding all the counters it will receive.
  
**PRINT** (1 File, N concurrent writes)
The coordinator sends a PRINT type message to the leader server.
The leader makes a broadcast with the message to
server tree. When a process receives the message, it must print to a file:
SERVER <server_rank> HAS <records_count> RECORDS WITH <total_hours> HOURS
Then she herself forwards the message to her children. In the end, one must be implemented
convergecast to inform the leader that all other processes received the message.
After this is done, the leader also prints a message to the file:
LEADER SERVER <server_rank> HAS <records_count> RECORDS WITH <total_hours> HOURS
  
The coordinator sends a message of type PRINT to the leader client as well. Subsequently,
the same process takes place, but in the client process tree this time.
Each client process that receives the master message must print to a file:
CLIENT <client_rank> PROCESSED <request_count> REQUESTS
  
**Termination**
Coordinator sends a message of type <TERMINATE> to all other processes so that it informs them about it
system shutdown.
  
  
  
  
  
  
