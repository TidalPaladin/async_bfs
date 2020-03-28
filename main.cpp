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

#define DEBUG false

#define debug_print(fmt, ...) \
  do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

#define REPLY  0
#define DIST 1
#define ACK 1
#define NACK 0

#define MAX_DIST 10000

static int linkCount = 0;
static int msgCount = 0;
std::vector<std::vector<int>> adjacency;

struct Message {
  unsigned int msg; // 0->nack 1->ack when type is 0; distance from the root when type is 1
  unsigned int type; // 0->nack/ack; 1->distance
  unsigned int sender; // process id that sends this messgae
};

// To record the pipes of the links and whether receive ack/nack
struct Link {
	int read;
	int write;
	int target;
	int reply;
};

// For a certain link, put all the messages in a queue with their delay
struct QueuedMsg {
	struct Message msg;
	int delay;
};

struct Output {
	int pipe;
	int target;
	std::vector<QueuedMsg> que;
};

struct TreeLink {
	int node;
	int dist;
	int parent = -1;
	std::vector<int> children;
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
	node1_link.reply = 0;
	struct Output node1_output;
	node1_output.pipe = pipe12[1];
	node1_output.target = node2;
	
	struct Link node2_link;
	fcntl(pipe12[0], F_SETFL, O_NONBLOCK); // set non-block state
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

// Broadcast to all the neighbors except the parent
void broadcast(int except, int sender, std::vector<Output>* output, struct Message msg) {
	for(auto &it : *output) {
		if(it.target == except) {
			continue;
		}
		
		struct QueuedMsg queuedMsg;
		queuedMsg.msg = msg;
		queuedMsg.delay = ceil(((float)rand()/RAND_MAX) * 15); // set random delay time from 1 to 15
		// If it is not the only message, the delay time should be added with the previous one to keep FIFO
		if(!it.que.empty()) {
			queuedMsg.delay += it.que.end()->delay;
		}
		it.que.push_back(queuedMsg);
	}
}

// Check message queue for each neighbor
bool checkMsgQueue(int node, std::vector<Output>* output) {
	static int f = 0;
	bool flag = true;
	for(auto &it : *output) {
		if(!it.que.empty()) {
			flag = false;
			for(std::vector<QueuedMsg>::iterator iter = it.que.begin(); iter != it.que.end(); iter++) {
				iter->delay--;
				//if(node == 2)printf("node 2: %d - %d - %d\n", it.target, iter->msg.type, iter->delay);
				// If the delay is 0, then it is time to send the message
				if(iter->delay == 0) {
					write(it.pipe, &iter->msg, sizeof(struct Message));
					//if(iter->msg.type == 1 && (node == 7 || node == 3 || node == 2))printf("process %d send msg to %d\n", node, it.target);
					//else if(iter->msg.type == 0 && (node == 7 || node == 3 || node == 2))printf("process %d ack/nack to %d\n", node, it.target);
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

// send a message to a single node, usually a ack/nack
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

void update_adjacency(TreeLink links){
  int node = links.node;
	for(auto &iter : links.children) {
    adjacency[node].push_back(iter);
  }
  if(links.parent >= 0) {
    adjacency[node].push_back(links.parent);
  }
}

void print_adjacency(){
  printf("Adjacency List:\n");
  int pid = 0;
	for(auto &iter : adjacency) {
    printf("%d -> {", pid);
    for(auto &childiter : iter) {
      printf("%d, ", childiter);
    }
    printf("}\n");
    pid++;
  }
}

TreeLink init_tree(std::vector<struct Link>* neighbors, std::vector<struct Output>* output, int node) {
  printf("Initiating BFS tree construction from process %d...\n", node);

  // init vars and send outgoing messages 
  struct TreeLink result;
  result.node = node;
	struct Message inputMsg; 
	struct Message outputMsg;
  outputMsg.msg = result.dist;
  outputMsg.type = DIST; 
  outputMsg.sender = node;
  broadcast(-1, node, output, outputMsg); // broadcast the distance to all the neighbors

  // wait for replies from neighbors
  while(1) {
    checkMsgQueue(node, output);
    if(readMsg(neighbors, &inputMsg)) {

      // receive ack
      if(inputMsg.type == REPLY) {
        for(auto &iter : *neighbors) {
          if(iter.target == inputMsg.sender) {
            iter.reply = 1;
            debug_print("Root %d receive ack from %d\n", node, inputMsg.sender);
            break;
          }

        }
        if(inputMsg.type == REPLY && inputMsg.msg == ACK) {
          debug_print("process %d ack from %d\n", node, inputMsg.sender);
          result.children.push_back(inputMsg.sender);
        }
      }
      else {
          debug_print("Root received dist from %d\n", inputMsg.sender);
      }
    }
    // if receive ack from all the neighbors, then it is time to terminate
    if(checkReply(-1, neighbors)) {
      printf("Initiator finished tree construction\n");
      return result;
    }
  }
}


TreeLink explore_loop(std::vector<struct Link>* neighbors, std::vector<struct Output>* output, int node) {
  printf("Initiating BFS tree helper from process %d...\n", node);
  // init vars and send outgoing messages 
  struct TreeLink result;
  result.node = node;
  result.dist = MAX_DIST;
	struct Message inputMsg; 
	struct Message outputMsg;
  outputMsg.msg = result.dist;
  outputMsg.type = DIST; 
  outputMsg.sender = node;
  //broadcast(-1, node, output, outputMsg); // broadcast the distance to all the neighbors
  bool done = false;

  while(1) {
    if(readMsg(neighbors, &inputMsg)) {
      if(inputMsg.type == DIST) {
        debug_print("Node %d got DIST from node %d\n", node, inputMsg.sender);
        // parent change
        if(result.dist > inputMsg.msg + 1) {
          //if(node == 7 || node == 3 || node == 2)printf("process %d receive %d from %d\n", node, inputMsg.msg, inputMsg.sender);
          result.dist = inputMsg.msg + 1; // reset the distance
          outputMsg.msg = NACK;
          outputMsg.sender = node;
          outputMsg.type = REPLY;
          reply(result.parent, node, output, outputMsg); // send nack to previous parent
          result.parent = inputMsg.sender;
          outputMsg.msg = result.dist;
          outputMsg.sender = node;
          outputMsg.type = DIST;
          // reset the reply for the new broadcast
          for(auto &iter : *neighbors) {
            iter.reply = 0;
          }
          broadcast(result.parent, node, output, outputMsg); // broadcast the new distance
        }
        // invalid message
        else if(inputMsg.sender != result.parent){
          //if(node == 7 || node == 3 || node == 2)printf("process %d reject %d from %d\n", node, inputMsg.msg, inputMsg.sender);
          struct Message outputMsg;
          outputMsg.msg = NACK;
          outputMsg.sender = node;
          outputMsg.type = REPLY;
          reply(inputMsg.sender, node, output, outputMsg); // send a nack
          debug_print("process %d nack to %d\n", node, inputMsg.sender);
        }
      }
      // receive an ack/nack
      else if(inputMsg.type == REPLY) {
        for(auto &iter : *neighbors) {
          if(iter.target == inputMsg.sender) {
            iter.reply = 1; // record the reply
            //if(node == 7 || node == 3 || node == 2)printf("process %d receive ack/nack from %d\n", node, inputMsg.sender);
            break;
          }
        }

        if(inputMsg.msg == ACK) {
          debug_print("process %d ack from %d\n", node, inputMsg.sender);
          result.children.push_back(inputMsg.sender);
        }
        else {
          debug_print("process %d nack from %d\n", node, inputMsg.sender);
        }
      }
    }

    // if get ack/nack for all the messages
    if(!done && checkReply(result.parent, neighbors)) {
      outputMsg.msg = ACK;
      outputMsg.sender = node;
      outputMsg.type = REPLY;
      reply(result.parent, node, output, outputMsg);
      //printf("process %d ack to %d\n", node, result.parent);
      debug_print("%d - %d\n", node, result.parent);
      printf("Node %d distance: %d\n", node, result.dist);
      done = true;
      update_adjacency(result);

      while(!checkMsgQueue(node, output)){
        continue;
      }
      //return result;
    }
    checkMsgQueue(node, output);
  }
}

/***********************************************
 * Process code
 * Thread entry point, called on thread creation 
 ***********************************************/
void thread_method(std::vector<struct Link>* neighbors, std::vector<struct Output>* output, bool isRoot, int node) {
  struct TreeLink result;
	
	if(isRoot) {
    result = init_tree(neighbors, output, node);
    update_adjacency(result);
	} 
	else {
    result = explore_loop(neighbors, output, node);
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
  adjacency = std::vector<std::vector<int>>(n, std::vector<int>());
	
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
  threads[root]->join();	

  printf("\n");
  print_adjacency();
  printf("Total message count: %d\n", msgCount);
  printf("Average messages per link: %f\n", msgCount * 1.0 / linkCount);
	return 0; // program finished
}
