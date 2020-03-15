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
#include <math.h>
#include <thread>
#include <iostream>
#include <vector>

#define NO_MESSAGE  0
#define EXPLORING 1
#define RETURNING_SUCCESS 2
#define I_AM_LEADER 3

struct Message {
  unsigned int mes;
};

struct QueuedMessage{
	struct Message* message;
	int delay;
	int pipe;
};

struct Link {
	int read;
	int write;
};

// method headers
void createLineBetween(int node1, int node2, std::vector<struct Link>** links);
void sendMessage(struct Message* message, int fd, std::vector<struct QueuedMessage>*); 
void checkMessageQueue(std::vector<struct QueuedMessage>*);

/***********************************************
 * Process code
 * Thread entry point, called on thread creation 
 ***********************************************/
void thread_method(std::vector<struct Link>* neighbors, bool isRoot){
	
}


void sendMessage(struct Message* message, int fd, std::vector<struct QueuedMessage>* queue){
	struct QueuedMessage qm;
	qm.message = message;
	qm.pipe = fd;
	qm.delay = ceil(((float)rand()/RAND_MAX) * 15);
	queue->push_back(qm);
} 

void checkMessageQueue(std::vector<struct QueuedMessage>* queue){
	if(queue->size() > 0){
		queue->at(0).delay--;
		if(queue->at(0).delay == 0){
			write(queue->at(0).pipe, queue->at(0).message, sizeof(struct Message));
			queue->erase(queue->begin());
		}
	}
}

void createLinkBetween(int node1, int node2, std::vector<struct Link>** links){
	int pipe12[2];
	int pipe21[2];
	pipe(pipe12);
	pipe(pipe21);

	struct Link node1_link;
	node1_link.read = pipe21[0];
	node1_link.write = pipe12[1];
	struct Link node2_link;
	node2_link.read = pipe12[0];
	node2_link.write = pipe21[1];

	links[node1]->push_back(node1_link);
	links[node2]->push_back(node2_link);
}

int main(int argc, char** argv){
	
	int n = 4;

	/***********************************
	 * Process setup and initialization
	 ***********************************/
	//create vectors of links to neighbors
	std::vector<struct Link>* links[n];
	std::vector<struct Link>** links_ref = (std::vector<struct Link>**)&links;
	for(int i = 0; i < n; i++){
		links[i] = new std::vector<struct Link>();
	}

	// Create sample network topology	
	createLinkBetween(0, 1, links_ref);
	createLinkBetween(0, 2, links_ref);
	createLinkBetween(2, 3, links_ref);
	createLinkBetween(3, 1, links_ref);
//	0--1
//	|  |
//	2--3

	std::thread* threads[n]; //pointers to every thread process
	// Create threads, pass in vector of neighbor links
	for(int i = 0; i < n; i++){
		threads[i] = new std::thread(thread_method, links[i], i==0);
	}


	// Collect all threads as they terminate
	for(int i = 0; i < n; i++){
		threads[i]->join();	
	}

	return 0;
}
