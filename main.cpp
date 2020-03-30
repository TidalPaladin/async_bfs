/* main.cpp
 * Distributed Computing - CS6380
 * Project 2 - AsynchBFS
 *
 * Group members:
 * Scott Waggener
 * Jonathan White
 * Zixuan Yang
 */


#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <math.h>
#include "fileops.cpp"

#define DEBUG 0

#define debug_print(fmt, ...) \
  do { if (DEBUG) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#define test_print(fmt, ...) \
  do { if (DEBUG > 1) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

// msg type
#define REPLY  0
#define DIST 1
#define TERM 2

// reply type
#define ACK 1
#define NACK 0
#define NO_REPLY -1

#define MAX_DIST 10000

int linkCount = 0;
int msgCount = 0;

struct Message {
  unsigned int msg; // 0->nack 1->ack when type is 0; distance from the root when type is 1
  unsigned int type; // 0->nack/ack; 1->distance 2->termination
  unsigned int sender; // process id that sends this messgae
};

// To record the pipes of the links and whether receive ack/nack
struct Link {
	int read;
	int write;
	int target;
};

// For a certain link, put all the messages in a queue with their delay
struct QueuedMsg {
	struct Message msg;
	int delay;
};

struct Output {
	int pipe;
	int target;
	int reply; // -1-> no reply 1->ack 0->nack
	std::vector<QueuedMsg> que;
};

// Create two pipes and two output queues between two nodes
void createLinkBetween(int node1, int node2, std::vector<struct Link>** links, std::vector<Output>** output) {

	// Create two pipes
	int pipe12[2];
	int pipe21[2];
	if(pipe(pipe12) || pipe(pipe21)) {
		throw -1;
	}
	
	struct Link node1_link;
	fcntl(pipe21[0], F_SETFL, O_NONBLOCK); // set non-block state
	node1_link.read = pipe21[0];
	node1_link.write = pipe12[1];
	node1_link.target = node2;
	struct Output node1_output;
	node1_output.pipe = pipe12[1];
	node1_output.target = node2;
	node1_output.reply = NO_REPLY;
	
	struct Link node2_link;
	fcntl(pipe12[0], F_SETFL, O_NONBLOCK); // set non-block state
	node2_link.read = pipe12[0];
	node2_link.write = pipe21[1];
	node2_link.target = node1;
	struct Output node2_output;
	node2_output.pipe = pipe21[1];
	node2_output.target = node1;
	node2_output.reply = NO_REPLY;

	links[node1]->push_back(node1_link);
	output[node1]->push_back(node1_output);
	links[node2]->push_back(node2_link);
	output[node2]->push_back(node2_output);
	
	linkCount++;
	
	test_print("create link between %d and %d\n", node1, node2);
}

// Broadcast to all the neighbors except the parent
void broadcast(int parent, int node, std::vector<Output>* output, struct Message msg) {
	for(auto &it : *output) {
		if(it.target == parent) {
			continue;
		}
		struct QueuedMsg queuedMsg;
		queuedMsg.msg = msg;
		queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15); // set random delay time from 1 to 15
		// If it is not the only message, the delay time should be added with the previous one to keep FIFO
		if(!it.que.empty()) {
			queuedMsg.delay += (--it.que.end())->delay;
		}
		it.que.push_back(queuedMsg);
	}
}

void broadcast_to_children(int node, std::vector<Output>* output, struct Message msg) {
	for(auto &it : *output) {
		if(it.reply != ACK) {
			continue;
		}
		struct QueuedMsg queuedMsg;
		queuedMsg.msg = msg;
		queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15); // set random delay time from 1 to 15
		// If it is not the only message, the delay time should be added with the previous one to keep FIFO
		if(!it.que.empty()) {
			queuedMsg.delay += (--it.que.end())->delay;
		}
		it.que.push_back(queuedMsg);
	}
}

// Check message queue for each neighbor
bool checkMsgQueue(int node, std::vector<Output>* output) {
	static int f = 0;
	bool flag = false;
	for(auto &it : *output) {
		if(!it.que.empty()) {
			flag = true;
			for(std::vector<QueuedMsg>::iterator iter = it.que.begin(); iter != it.que.end(); iter++) {
				iter->delay--;
				if(iter->delay == 0) {
					write(it.pipe, &iter->msg, sizeof(struct Message));
					test_print("node %d send %d to %d, type: %d\n", node, iter->msg.msg, it.target, iter->msg.type);
					it.que.erase(iter);
					iter--;
					msgCount++;
				}
			}
		}
	}
	return flag;
}

// Check all the read pipes and see if any message comes in
bool readMsg(std::vector<struct Link>* links, struct Message* msg) {
	for(auto &it : *links) {
		int flag = read(it.read, msg, sizeof(struct Message));
		if(flag != EOF) {
			return true;
		}
	}
	return false;
} 

// Check if receive the ack/nack for all the broadcast messages
bool checkReply(int parent, std::vector<Output>* output) {
	for(auto &iter : *output) {
		if(iter.target == parent) {
			continue;
		}
		else if(iter.reply == NO_REPLY) {
			return false;
		}
	}
	return true;
}

// send a message to a single node, usually a ack/nack
void reply(int target, int node, std::vector<Output>* output, struct Message msg) {
	for(auto &iter : *output) {
		if(iter.target == target) {
			struct QueuedMsg queuedMsg;
			queuedMsg.msg = msg;
			queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15);
			if(!iter.que.empty()) {
				queuedMsg.delay += (--iter.que.end())->delay;
			}
			iter.que.push_back(queuedMsg);
			break;
		}
	}
}

// output the links to children
void printLinks(int node, std::vector<Output>* output) {
	for(auto &iter : *output) {
		if(iter.reply == ACK) {
			printf("%d - %d\n", node, iter.target);
		}
	}
}

/***********************************************
 * Process code
 * Thread entry point, called on thread creation 
 ***********************************************/
void thread_method(std::vector<struct Link>* neighbors, std::vector<struct Output>* output, bool isRoot, int node) {
	unsigned int dist;
	int parent = -1;
	bool running = true;
	bool done = false;
	bool sending = true;
	struct Message inputMsg; 
	struct Message outputMsg;
	int acks_received = 0;
	
	// root node
	if(isRoot) {
		dist = 0;
		outputMsg.msg = dist;
		outputMsg.type = DIST; 
		outputMsg.sender = node;
		broadcast(-1, node, output, outputMsg); // broadcast the distance to all the neighbors
		while(running || sending) {
			if(readMsg(neighbors, &inputMsg)) {
				// receive ack
				if(inputMsg.type == REPLY) {
					acks_received++;
					for(auto &iter : *output) {
						if(iter.target == inputMsg.sender) {
							iter.reply = inputMsg.msg;
							break;
						}
					}
				}
				else if(inputMsg.type == DIST){ // receive distance message, always send nack
					struct Message outputMsg;
					outputMsg.msg = NACK;
					outputMsg.sender = node;
					outputMsg.type = REPLY;
					reply(inputMsg.sender, node, output, outputMsg); // send a reply
				}
			}
			// if receive ack from all the neighbors, then it is time to terminate
			if(acks_received == neighbors->size() && running) {
				running = false;
				printf("Total messages: %d\n", msgCount);
				printf("Average messages: %f\n", msgCount * 1.0 / linkCount);
				printLinks(node, output);
				outputMsg.type = TERM;
				outputMsg.sender = node;
				broadcast_to_children(node, output, outputMsg); //send termination to children
			}
			sending = checkMsgQueue(node, output);
		}
	} 
	// non-root node
	else {
		dist = MAX_DIST;
		while(running || sending) {
			if(readMsg(neighbors, &inputMsg)) {
				test_print("node %d receive %d from %d, type:%d\n", node, inputMsg.msg, inputMsg.sender, inputMsg.type);
				// receive distance
				if(inputMsg.type == DIST) {
					// parent change
					if(dist > inputMsg.msg + 1) {
						dist = inputMsg.msg + 1; // reset the distance
						outputMsg.msg = NACK;
						outputMsg.sender = node;
						outputMsg.type = REPLY;
						if(parent != -1){
							reply(parent, node, output, outputMsg); // send nack to previous parent
							acks_received -= neighbors->size() - 1;// need as many acks as neighbors minus the parent
						}
						parent = inputMsg.sender;
						outputMsg.msg = dist;
						outputMsg.sender = node;
						outputMsg.type = DIST;
						broadcast(parent, node, output, outputMsg); // broadcast the new distance
						// reset the reply for the new broadcast
						for(auto &iter : *output) {
							iter.reply = NO_REPLY;
						}
						done = false;
					}
					// invalid message
					else {
						struct Message outputMsg;
						outputMsg.msg = NACK;
						outputMsg.sender = node;
						outputMsg.type = REPLY;
						reply(inputMsg.sender, node, output, outputMsg); // send a reply
					}
				}
				// receive an ack/nack
				else if(inputMsg.type == REPLY) {
					acks_received++;
					test_print("Node %d has %d of %d acks\n", node, acks_received, neighbors->size()-1);
					for(auto &iter : *output) {
						if(iter.target == inputMsg.sender) {
							iter.reply = inputMsg.msg; // record the reply
							break;
						}
					}
				}
				// receive termination
				else if(inputMsg.type == TERM) {
					running = false;
					printLinks(node, output);
					outputMsg.type = TERM;
					outputMsg.sender = node;
					broadcast_to_children(node, output, outputMsg);
				}
			}
			// if get ack/nack for all the messages
			if(acks_received == neighbors->size()-1 && !done) {
				outputMsg.msg = ACK;
				outputMsg.sender = node;
				outputMsg.type = REPLY;
				reply(parent, node, output, outputMsg);
				debug_print("node %d send an ack to %d\n", node, parent);
				done = true;
			}
			sending = checkMsgQueue(node, output);
		}
	}
}

int main(int argc, char** argv)
{
	// Read n and q from input file
	if(argc != 2) {
	    printf("Usage: %s <input.txt>\n", argv[0]);
	    return 0;
	}
	const char* filepath = argv[1];
	struct Meta input;

	input = get_meta(filepath);
	int n = input.num;
	int root = input.root;
	
	//create vectors of links to neighbors
	std::vector<struct Link>* links[n];
	std::vector<struct Link>** links_ref = (std::vector<struct Link>**)&links;
	for(int i = 0; i < n; i++){
		links[i] = new std::vector<struct Link>();
	}
	
	//create vectors of output queues to neighbors
	std::vector<struct Output>* outputs[n];
	std::vector<struct Output>** outputs_ref = (std::vector<struct Output>**)&outputs;
	for(int i = 0; i < n; i++){
		outputs[i] = new std::vector<struct Output>();
	}
	
	for(int i = 0; i < n; i++) {
		for(int j = 0; j < n; j++) {
			test_print("%d", input.matrix[i][j]);
		}
		test_print("\n");
	}

	// create pipes
	try {
		for(int i = 0; i < n; i++) {
			for(int j = i + 1; j < n; j++) {
				if(input.matrix[i][j]) {
					createLinkBetween(i, j, links_ref, outputs_ref);
				}
			}
		}
	}
	catch(int e) {
		printf("Create pipes failed!");
		return e;
	}
	
	
	std::thread* threads[n]; //pointers to every thread process
	// Create threads, pass in vector of neighbor links
	for(int i = 0; i < n; i++){
		threads[i] = new std::thread(thread_method, links[i], outputs[i], i==root, i);
	}
	
	// Collect all threads as they terminate
	for(int i = 0; i < n; i++){
		threads[i]->join();	
	}

	return 0; // program finished
}
