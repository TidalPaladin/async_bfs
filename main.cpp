/* main.cpp
 * Distributed Computing - CS6380
 * Project 1 - HS algorithm simulation
 *
 * Group members:
 * Scott Waggener
 * Jonathan White
 * Zixaun Yang
 */


#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <vector>
#include "fileops.cpp"

#define NO_MESSAGE  0
#define EXPLORING 1
#define RETURNING_SUCCESS 2
#define I_AM_LEADER 3

struct Message {
  unsigned int id;
  unsigned int ttl;
  unsigned int status;
};


/***********************************************
 * Process code
 * Thread entry point, called on thread creation 
 ***********************************************/
void thread_method(int id, int readLeft_fd, int writeLeft_fd, int readRight_fd, int writeRight_fd, int readMaster_fd, int writeMaster_fd){
	
	
	// Declare local vairables
	bool running = true; // condition for primary loop
	char master_message; // buffer variable for master messages
	struct Message right_message_in, right_message_out, left_message_in, left_message_out; // variables store store incoming and outgoing messages from pipe buffers
	int phase = 0; // start at phase 0
	int message_ttl = 1 << phase; //ttl to put on outgoing messages
	int phase_time_left = message_ttl*2; // time to wait for phase to finish
	bool candidate = true; // all processes are initially candidates
	int leader = -1;// id of leader when found, -1 if leader is not known

	
	/****************************
	 * Initial step of algorithm
	 ****************************/
	// Send initial messages to left and right neighbor
	//initialize initial messages
	right_message_out.id = id;
	right_message_out.ttl = message_ttl;
	right_message_out.status = EXPLORING;
	left_message_out.id = id;
	left_message_out.ttl = message_ttl;
	left_message_out.status = EXPLORING;
	//send initial messages
	write(writeRight_fd, &right_message_out, sizeof(struct Message));
	write(writeLeft_fd, &left_message_out, sizeof(struct Message));
	


	/******************************************************
	 * Main loop of algorithm, each iteration is one round
	 ******************************************************/
	while(running){ // primary loop

		// Wait for "go ahead" from master to start round
		read(readMaster_fd, &master_message, sizeof(char)); // wait for start round message from master
		if(master_message == -1){ // if master says to terminate
			running = false; // terminate
			continue;
		}

		/*****************
		 * Start of round
		 *****************/

		
		/*******************************
		 * Read messages from neighbors
		 *******************************/
		// read messages from neighbors that were sent last round, if no message was sent, an empty message is returned
		read(readRight_fd, &right_message_in, sizeof(struct Message));
		read(readLeft_fd, &left_message_in, sizeof(struct Message));
		
		// reset out message values
		right_message_out.id = id;
		right_message_out.status = NO_MESSAGE;
		left_message_out.id = id;
		left_message_out.status = NO_MESSAGE;


		if(leader != -1){}// the leader has already been found, no need to do anything
		//check if I became the leader
		else if(right_message_in.status == EXPLORING && right_message_in.id == id && left_message_in.status == EXPLORING && left_message_in.id == id){// if both probes come all the way around, must be the leader
			leader = id;// found the leader, it is me
			right_message_out.id = id;
			right_message_out.status = I_AM_LEADER;
			left_message_out.id = id;
			left_message_out.status = I_AM_LEADER;
			printf("Leader: %d\n",id); // Leader is found, output leader id
		}
		//check if leader is found on right
		else if(right_message_in.status == I_AM_LEADER){
			leader = right_message_in.id;
			right_message_out.status = NO_MESSAGE;
			left_message_out.id = leader;
			left_message_out.status = I_AM_LEADER;
		}
		//check if leader is found on left
		else if(left_message_in.status == I_AM_LEADER){
			leader = left_message_in.id;
			left_message_out.status = NO_MESSAGE;
			right_message_out.id = leader;
			right_message_out.status = I_AM_LEADER;
		}
		else{ // otherwise do one round of the current phase
			phase_time_left--;
			
			/******************
			 * If end of phase
			 ******************/
			if(phase_time_left == 0){// end of phase
				//reset values for new phase
				phase++;
				message_ttl = 1 << phase;
				phase_time_left = message_ttl*2;


				// if both probes return sucessfully, stay in the running
				if(right_message_in.status == RETURNING_SUCCESS && right_message_in.id == id && left_message_in.status == RETURNING_SUCCESS && left_message_in.id == id){
					//send out next exploration
					right_message_out.id = id;
					right_message_out.ttl = message_ttl;
					right_message_out.status = EXPLORING;
					left_message_out.id = id;
					left_message_out.ttl = message_ttl;
					left_message_out.status = EXPLORING;
				}
				else{ // must be a larger id in the neighborhood, drop out of running
					candidate = false;// lost, now shut up
					right_message_out.status = NO_MESSAGE;
					left_message_out.status = NO_MESSAGE;
				}
			}
			/**************************************************
			 * Iterate one round of message passing/forwarding
			 *************************************************/
			else{// phase not over
				
				//check right message
				if(right_message_in.status == EXPLORING && right_message_in.id > id){// if explore message has larger id than this id
					if(right_message_in.ttl == 1){ //send message back successfully if it reached the end of its time to live
						right_message_out.id = right_message_in.id;
						right_message_out.status = RETURNING_SUCCESS;
					}
					else{ // if message still has more ttl, forward to next node
						left_message_out.id = right_message_in.id;
						left_message_out.ttl = right_message_in.ttl-1; // decrease message lifetime by one round
						left_message_out.status = EXPLORING;
					}
				}
				else if(right_message_in.status == RETURNING_SUCCESS){// if the message is a returning message, forward it along its path
					left_message_out.id = right_message_in.id;
					left_message_out.status = RETURNING_SUCCESS;
				}
				// If the message is an explore message with a smaller id, it is discard and ignored
				
				//check left message
				if(left_message_in.status == EXPLORING && left_message_in.id > id){// if explore message has larger id than this id
					if(left_message_in.ttl == 1){ //send message back successfully if it reached the end of its lifetime
						left_message_out.id = left_message_in.id;
						left_message_out.status = RETURNING_SUCCESS;
					}
					else{ // if message still has more ttl, forward to next node
						right_message_out.id = left_message_in.id;
						right_message_out.ttl = left_message_in.ttl-1;
						right_message_out.status = EXPLORING;
					}
				}
				else if(left_message_in.status == RETURNING_SUCCESS){// if the message is a returning message, forward it along its path
					right_message_out.id = left_message_in.id;
					right_message_out.status = RETURNING_SUCCESS;
				}
				// If the message is an explore message with a smaller id, it is discard and ignored

			}
		}

		/*****************************
		 * Send messages to neighbors
		 *****************************/
		if(right_message_out.status != NO_MESSAGE){ // write message to right if have something to say
			write(writeRight_fd, &right_message_out, sizeof(struct Message));	
		}
		else{// if no message to send, write an empty message to the pipe
			right_message_out.id = -1;
			right_message_out.ttl = -1;
			right_message_out.status = NO_MESSAGE;
			write(writeRight_fd, &right_message_out, sizeof(struct Message));
		}
		if(left_message_out.status != NO_MESSAGE){ // write message to left if have something to say
			write(writeLeft_fd, &left_message_out, sizeof(struct Message));
		}
		else{// if no message to send, write an empty message to the pipe
			left_message_out.id = -1;
			left_message_out.ttl = -1;
			left_message_out.status = NO_MESSAGE;
			write(writeLeft_fd, &left_message_out, sizeof(struct Message));
		}

		/***************
		 * End of round
		 ***************/
		if(leader == -1){ // if leader is unknown
			master_message = 0; // tell master that we are not ready to terminate
		}
		else{
			master_message = 1; // found leader, ready to terminate, algorithm is finished
		}
		write(writeMaster_fd, &master_message, sizeof(char)); // tell master that this thread finished the round
	}
	
}


int main(int argc, char** argv){
	
	// Read n and q from input file
  if(argc != 2) {
    printf("Usage: %s <input.txt>\n", argv[0]);
    return 0;
  }
  const char* filepath = argv[1];
  std::vector<int> pids = *get_meta(filepath);
  const int n = pids.size();


	/***********************************
	 * Process setup and initialization
	 ***********************************/
	// Declare variables to pass to processes
	int id; // unique id
	int readLeft_pipe[2], writeLeft_pipe[2], readRight_pipe[2], writeRight_pipe[2]; // Pipes for communication with neighbors
	int readMaster_fd, writeMaster_fd; // pipes for communication with master
	
	//Declare master variables
	int process_pipes[n][2]; // pipes that master and processes use to sync rounds
	std::thread* threads[n]; // pointer to every process thread
	
	// Create pipes to connect first and last process
	int lastRR_fd, lastWR_fd; // pipes that will wrap around between the first and last process in the list
	pipe(writeRight_pipe); // create wraparound pipes
	pipe(readRight_pipe);
	lastWR_fd = writeRight_pipe[1]; //  store wrap-around pipe file discriptors
	lastRR_fd = readRight_pipe[0];
	

	// Set up processes
	for(int i = 0; i < n; i++){//init n many processes
		
		id = pids[i]; // set unique id to be passed to process

		int temp_pipes[4][2]; // pipes to be created for neighbor communication
		for(int p = 0; p < 4; p++){
			if(pipe(temp_pipes[p]) < 0){ // create neighbor pipes
				printf("Error creating pipes\n");
				return -1;
			}
		}
		
		// Set master's master-process pipe fds
		process_pipes[i][0] = temp_pipes[0][0]; // master read from process
		process_pipes[i][1] = temp_pipes[1][1]; // master write to process
		
		// Set process's master-process pipe fds
		writeMaster_fd = temp_pipes[0][1]; // process write to master
		readMaster_fd = temp_pipes[1][0]; // process read from master

		// Set process's left neighbor pipes to the other ends of the previous process's right neighbor pipes
		readLeft_pipe[0] = writeRight_pipe[0];
		writeLeft_pipe[1] = readRight_pipe[1];
		
		if(i == n-1){ // if the last process, set right neighbor pipes to wraparound pipes saved earlier
			readRight_pipe[0] = lastRR_fd;
			writeRight_pipe[1] = lastWR_fd;
		}
		else{ // if not the last process, set right neighbor pipes to newly created pipes
			readRight_pipe[0] = temp_pipes[2][0];
			readRight_pipe[1] = temp_pipes[2][1];
			writeRight_pipe[0] = temp_pipes[3][0];
			writeRight_pipe[1] = temp_pipes[3][1];
		}
		
		// Create new thread, pass in variables
		threads[i] = new std::thread(thread_method, id, readLeft_pipe[0], writeLeft_pipe[1], readRight_pipe[0], writeRight_pipe[1], readMaster_fd, writeMaster_fd);
	}// end of setting up processes



	// Declare master local variables
	bool running = true; // conditional for primary loop
	char message = 0; // variable to use as buffer when reading/write with pipes
	int processStatusTotal = 0; // values received from processes, when they all send 1 they have a leader and are ready to terminate


	/**********************************************************
	 * Master loop, it iteration is one round of the algorithm
	 **********************************************************/
	while(running){ // master primary loop
	

		message = 0;// tell processes to "go ahead" and start the round
		for(int i = 0; i < n; i++){ // send start round message to all processes
			write(process_pipes[i][1], &message, sizeof(char));
		}

		processStatusTotal = 0;// reset count to 0
		for(int i = 0; i < n; i++){// wait for all process to end round
			read(process_pipes[i][0], &message, sizeof(char));// receive "done with round" message from process
			processStatusTotal += message;// add recieved message to total. 0 means done with round, 1 means leader found, ready to terminate
		}
		
		// termination detection
		if(processStatusTotal == n){// when everyone has found the leader, terminate all processes
			running = false;
			message = -1; // -1 message tells processes they can terminate
			for(int i = 0; i < n; i++){ // send terminate message to all processes
				write(process_pipes[i][1], &message, sizeof(char));
			}
		}
	}
	
	//  collects all threads as they terminate
	for(int i = 0; i < n; i++){
		threads[i]->join();
	}

	// Program output comes from leader thread which prints its id
	return 0; // program finished
}
