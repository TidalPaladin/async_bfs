#include <stdio.h>
#include <cstdlib>
#include <vector>

/*
 * Reads PID metadata from a file path. File is expected to be
 * a newline separated list of unique integers.
 *
 * @param filepath  The filepath to read from
 *
 * @return  Vector pointer of PIDs as read from `filepath`, or NULL 
 *          if error reading file
 */
std::vector<int> *get_meta(const char* filepath) {
  // try opening file
  FILE *fp = fopen(filepath, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error opening file '%s'\n", filepath);
    perror("fopen");
    return NULL;
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  std::vector<int> *file_contents = new std::vector<int>();

  // read ints line by line from file into array
  int pos = 0;
  while ((read = getline(&line, &len, fp)) != -1) {
    file_contents->push_back(atoi(line));
    pos++;
  }

  fclose(fp);
  if (line) {
      delete line;
  }
  return file_contents;
}

