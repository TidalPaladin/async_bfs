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
#include "fileops.cpp"

#define NO_MESSAGE  0
#define EXPLORING 1
#define RETURNING_SUCCESS 2
#define I_AM_LEADER 3

int main()
{
	//////
	int num, root;
	Meta* input = new Meta();
	std::vector<std::vector<int>> matrix;
	input = get_meta("input.txt");
	for(int i = 0; i < matrix.size(); i++) {
		for(int j = 0; j < matrix[i].size(); j++) {
			std::cout << input->matrix[i][j];
		}
		std::cout << std::endl;
	}
	return 0; // program finished
}
