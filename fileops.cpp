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
	int **matrix;
};
 
struct Meta* get_meta(const char* filepath) {
  // try opening file
  FILE *fp = fopen(filepath, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file '%s'\n", filepath);
    perror("fopen");
    return NULL;
  }
  
  Meta* file_contents = new Meta();
  int num, root;
  
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  
  if((read = getline(&line, &len, fp)) != -1) {
  	num = atoi(line);
  }
  
  if((read = getline(&line, &len, fp)) != -1) {
  	root = atoi(line);
  }

  // read ints line by line from file into array
  int pos = 0;
  char *p;
  int **matrix = new int*[num];
  for(int i = 0; i < num; i++) {
  	matrix[i] = new int[num];
  }
   
  
  while ((read = getline(&line, &len, fp)) != -1) {
  	int *temp = new int[file_contents->num];
  	p = strtok(line, " ");
  	int i = 0;
  	while(p) {
  		std::cout << *p << std::endl;
  		matrix[pos][i] = *p;
  		p = strtok(NULL, " ");
  		i++;
	}
	//matrix.push_back(temp);
    pos++;
  }


  fclose(fp);
  if (line) {
      delete line;
  }
  
  file_contents->num = num;
  file_contents->root = root;  
  file_contents->matrix = matrix;
  return file_contents;
}

