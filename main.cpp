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

#define NO_MESSAGE  0
#define EXPLORING 1
#define RETURNING_SUCCESS 2
#define I_AM_LEADER 3

#define MAX_DIST 10000

int linkCount = 0;
int msgCount = 0;

struct Message {
  unsigned int msg; // 0->nack 1->ack when type is 0; distance from the root when type is 1
  unsigned int type; // 0->nack/ack; 1->distance
  unsigned int sender; 
};

struct Link {
	int read;
	int write;
	int target;
	int reply;
};

struct QueuedMsg {
	struct Message msg;
	int delay;
};

struct Output {
	int pipe;
	int target;
	std::vector<QueuedMsg> que;
};



void createLinkBetween(int node1, int node2, std::vector<struct Link>** links, std::vector<Output>** output) {
	int pipe12[2];
	int pipe21[2];
	pipe(pipe12);
	pipe(pipe21);

	struct Link node1_link;
	fcntl(pipe21[0], F_SETFL, O_NONBLOCK); 
	node1_link.read = pipe21[0];
	node1_link.write = pipe12[1];
	node1_link.target = node2;
	node1_link.reply = 0;
	struct Output node1_output;
	node1_output.pipe = pipe12[1];
	node1_output.target = node2;
	
	struct Link node2_link;
	fcntl(pipe12[0], F_SETFL, O_NONBLOCK); 
	node2_link.read = pipe12[0];
	node2_link.write = pipe21[1];
	node2_link.target = node1;
	node2_link.reply = 0;
	struct Output node2_output;
	node2_output.pipe = pipe21[1];
	node2_output.target = node1;

	links[node1]->push_back(node1_link);
	output[node1]->push_back(node1_output);
	links[node2]->push_back(node2_link);
	output[node2]->push_back(node2_output);
	
	linkCount++;
	
	printf("create link between %d and %d\n", node1, node2);
}

void broadcast(int except, int sender, std::vector<Output>* output, struct Message msg) {
	for(auto &it : *output) {
		if(except != -1) {
			if(it.target == except) {
				continue;
			}
		}
		struct QueuedMsg queuedMsg;
		queuedMsg.msg = msg;
		queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15);
		if(!it.que.empty()) {
			queuedMsg.delay += it.que.end()->delay;
		}
		it.que.push_back(queuedMsg);
		//write(it.pipe, &msg, sizeof(struct Message));
		//printf("%d is going to %d\n", sender, it.target);
	}
}

bool checkMsgQueue(int node, std::vector<Output>* output) {
	static int f = 0;
	bool flag = true;
	for(auto &it : *output) {
		if(!it.que.empty()) {
			flag = false;
			for(std::vector<QueuedMsg>::iterator iter = it.que.begin(); iter != it.que.end(); iter++) {
				iter->delay--;
				if(iter->delay == 0) {
					write(it.pipe, &iter->msg, sizeof(struct Message));
					//if(iter->msg.type == 1)printf("process %d send msg to %d\n", node, it.target);
					//else printf("process %d ack/nack to %d\n", node, it.target);
					it.que.erase(iter);
					iter--;
					msgCount++;
				}
			}
		}
	}
	return flag;
}

bool readMsg(std::vector<struct Link>* links, struct Message* msg) {
	for(auto &it : *links) {
		int flag = read(it.read, msg, sizeof(struct Message));
		if(flag != EOF) {
			return true;
		}
	}
	return false;
} 

bool checkReply(int parent, std::vector<struct Link>* links) {
	for(auto &iter : *links) {
		if(iter.target == parent) {
			continue;
		}
		else if(iter.reply == 0) {
			return false;
		}
	}
	return true;
}

void reply(int target, int node, std::vector<Output>* output, struct Message msg) {
	for(auto &iter : *output) {
		if(iter.target == target) {
			struct QueuedMsg queuedMsg;
			queuedMsg.msg = msg;
			queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15);
			if(!iter.que.empty()) {
				queuedMsg.delay += iter.que.end()->delay;
			}
			iter.que.push_back(queuedMsg);
			//write(iter.pipe, &msg, sizeof(struct Message));
			//printf("process %d ack/nack to %d\n", node, iter.target);
			break;
		}
	}
}

/***********************************************
 * Process code
 * Thread entry point, called on thread creation 
 ***********************************************/
void thread_method(std::vector<struct Link>* neighbors, std::vector<struct Output>* output, bool isRoot, int node) {
	/*std::string output = "process:\n";
	for(int i = 0; i < neighbors->size(); i++){
		output += std::to_string(neighbors->at(i).read);
		output += " ";
		output += std::to_string(neighbors->at(i).write);
		output += "\n";
	}
	std::cout << output;*/
	
	unsigned int dist;
	int parent = -1;
	bool running = true;
	bool send = true;
	struct Message inputMsg; 
	struct Message outputMsg;
	
	if(isRoot) {
		printf("Root: process %d running\n", node);
		dist = 0;
		outputMsg.msg = 0;
		outputMsg.type = 1; 
		outputMsg.sender = node;
		broadcast(-1, node, output, outputMsg);
		while(running) {
			send = checkMsgQueue(node, output);
			if(readMsg(neighbors, &inputMsg)) {
				if(inputMsg.type == 0) {
					for(auto &iter : *neighbors) {
						if(iter.target == inputMsg.sender) {
							iter.reply = 1;
							//printf("Root %d receive ack from %d\n", node, inputMsg.sender);
							break;
						}
					}
				}
			}
			if(checkReply(-1, neighbors)) {
				running = false;
				printf("Program terminated, the number of average messages is %f\n", msgCount * 1.0 / linkCount);
			}
		}
	} 
	else {
		dist = MAX_DIST;
		int flag = 1;
		while(1) {
			if(readMsg(neighbors, &inputMsg)) {
				if(inputMsg.type == 1) {
					if(dist > inputMsg.msg + 1) {
						//printf("process %d receive %d from %d\n", node, inputMsg.msg, inputMsg.sender);
						dist = inputMsg.msg + 1;
						parent = inputMsg.sender;
						outputMsg.msg = dist;
						outputMsg.sender = node;
						outputMsg.type = 1;
						broadcast(parent, node, output, outputMsg);
						for(auto &iter : *neighbors) {
							iter.reply = 0;
						}
					}
					else {
						//printf("process %d reject %d from %d\n", node, inputMsg.msg, inputMsg.sender);
						struct Message outputMsg;
						outputMsg.msg = 0;
						outputMsg.sender = node;
						outputMsg.type = 0;
						reply(inputMsg.sender, node, output, outputMsg);
						//printf("process %d nack to %d\n", node, inputMsg.sender);
					}
				}
				else if(inputMsg.type == 0) {
					for(auto &iter : *neighbors) {
						if(iter.target == inputMsg.sender) {
							iter.reply = 1;
							//printf("process %d receive ack/nack from %d\n", node, inputMsg.sender);
							break;
						}
					}
				}
			}
			if(checkReply(parent, neighbors) && running) {
				outputMsg.msg = 1;
				outputMsg.sender = node;
				outputMsg.type = 0;
				reply(parent, node, output, outputMsg);
				printf("%d - %d\n", node, parent);
				running = false;
			}
			send = checkMsgQueue(node, output);
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
	/*for(int i = 0; i < n; i++) {
		for(int j = 0; j < n; j++) {
			std::cout << input.matrix[i][j];
		}
		std::cout << std::endl;
	}*/
	
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
		for(int j = i + 1; j < n; j++) {
			if(input.matrix[i][j]) {
				createLinkBetween(i, j, links_ref, outputs_ref);
			}
		}
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

	////
	return 0; // program finished
}
