#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <cstring>

/*
 * Reads PID metadata from a file path. File is expected to be
 * a newline separated list of unique integers.
 *
 * @param filepath  The filepath to read from
 *
 * @return  Vector pointer of PIDs as read from `filepath`, or NULL 
 *          if error reading file
 */
 
struct Meta {
	int num;
	int root;
	std::vector<std::vector<int>> matrix;
};
 
struct Meta get_meta(const char* filepath) {
  struct Meta file_contents;
  int num, root;	
	
  // try opening file
  FILE *fp = fopen(filepath, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file '%s'\n", filepath);
    perror("fopen");
    return file_contents;
  }
  
  
  
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  
  if((read = getline(&line, &len, fp)) != -1) {
  	num = atoi(line);
  }
  
  if((read = getline(&line, &len, fp)) != -1) {
  	root = atoi(line);
  }

  // init the matrix 
  int pos = 0;
  char *p;
  int **matrix = new int*[num];
  
  while ((read = getline(&line, &len, fp)) != -1) {
  	std::vector<int> temp;
  	p = strtok(line, " ");
  	int i = 0;
  	while(p) {
  		//std::cout << *p << std::endl;
  		temp.push_back(atoi(p));
  		p = strtok(NULL, " ");
  		i++;
	}
	//for(auto iter : temp)std::cout << iter << std::endl;
	file_contents.matrix.push_back(temp);
    pos++;
  }


  fclose(fp);
  if (line) {
      delete line;
  }
  
  file_contents.num = num;
  file_contents.root = root;  
  return file_contents;
}

