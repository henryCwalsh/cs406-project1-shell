//TEST sync
// some needed h-files
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

/************************************ string formatting commands *************/
int  contains_only_ws          (char *buf);
void remove_special_characters (char *buf);
void trim_trailing_ws          (char *buf);
void trim_leading_ws           (char *buf);
void remove_duplicate_ws       (char *buf);

/************************************ path and exec processing   *************/
char **split_args_str     (char *str);

/************************************ Error Handling *************************/
void print_error(void);
/*****************************************************************************/
/************************************ the main event   ***********************/
/*****************************************************************************/
char *paths[100];
int path_count = 1;

int main(int argc, char *argv[]) {
  FILE *input = NULL;
  int interactive = 0;
  char *buf = NULL;
  size_t bufsize = 0;
  ssize_t lineLen;
  paths[0] = strdup("/bin");

  if(argc == 1) {
    input = stdin;
    interactive = 1;
  }
  else if(argc == 2) {
    input = fopen(argv[1], "r");
    if(input == NULL) {
      print_error();
      exit(1);
    }
    interactive = 0;
  }
  else{
    print_error();
    exit(1);
  }

  while(1) {
    if(interactive == 1) {
      printf("lsh> ");
      fflush(stdout);
    }

    lineLen = getline(&buf, &bufsize, input);
    if(lineLen == -1) {
      free(buf);
      exit(0);
    }

    remove_special_characters(buf);
    trim_trailing_ws(buf);
    trim_leading_ws(buf);
    remove_duplicate_ws(buf);
    if(contains_only_ws(buf)) {
      continue;
    }

    char **args = split_args_str(buf);
    char* cmd = args[1];

    if (cmd == NULL) {
      free(args);
      continue;
    }

    if(strcmp(cmd, "exit") == 0) { //handle exit command inside shell not through execv
      if(args[2] != NULL) {
        print_error();
      } else{
        free(args);
        exit(0);
      }
      free(args);
      continue;
    }

    if (strcmp(cmd, "cd") == 0) {
      if(args[3] != NULL) {
        print_error();
      } else if (args[2] == NULL) {
        print_error();
      }
      else {
        if(chdir(args[2]) != 0){
          print_error();
        }
      }
      free(args);
      continue;
    }

    if(strcmp(cmd, "path") == 0) {
      path_count = 0;
      int i = 2;
      while(args[i] != NULL && path_count < 100) {
        paths[path_count] = strdup(args[i]);
        path_count++;
        i++;
      }
      free(args);
      continue;
    }

    int redirect_index = -1;
    int redirect_error = 0;
    char *output_file = NULL;
    for(int i = 1; args[i] != NULL; i++) {
      if(strcmp(args[i], ">") == 0) {
        if(redirect_index != -1) {
          redirect_error = 1;
          break;
        }
        redirect_index = i;
      }
    }
    
    if(redirect_error) {
      print_error();
      free(args);
      continue;
    }

    if(redirect_index != -1) {
      if(redirect_index == 1){
        print_error();
        free(args);
        continue;
      }
      if(args[redirect_index + 1] == NULL || args[redirect_index + 2] != NULL) {
        print_error();
        free(args);
        continue;
      }

      output_file = args[redirect_index + 1];
      args[redirect_index] = NULL; // terminate the command arguments before the redirect symbol
    }

    char full_path[256];
    int found = 0;
    for(int i = 0; i < path_count; i++) {
      snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], cmd);
      if(access(full_path, X_OK) == 0) {
        found = 1;
        break;
      }
    }
    if(!found) {
      print_error();
      free(args);
      continue;
    }

    pid_t pid = fork();
    if (pid < 0) {
      print_error();
      free(args);
      continue;
    }

    else if (pid == 0) {
      if(output_file != NULL) {
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd < 0) {
          print_error();
          exit(1);
        }
        if(dup2(fd, STDOUT_FILENO) < 0) {
          print_error();
          close(fd);
          exit(1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
          print_error();
          close(fd);
          exit(1);
        }
        close(fd);
      }
      execv(full_path, &args[1]);
      print_error();
      exit(1);
    }

    else{
      waitpid(pid, NULL, 0); //parent waits for child to finish, then continues loop
    }

    free(args);
  }

  
  return 0;
}


/*****************************************************************************/
/************************************ some helpful code  *********************/
/*****************************************************************************/

//-- Checks a command string to see if it is only whitespace, returning 1
//-- if it is true.
int contains_only_ws(char *buf) {
  
  while(*buf) {
    switch (*buf) {
      case '\n':
      case ' ':
      case '\t': {
        buf++;
        break;
      }
      default: {
        return 0;
      }
    }
  }

  return 1;
}

//-- Change all special characters like newline and tab to characters. 
void remove_special_characters(char *buf) {

  while(*buf) {
    
    switch(*buf) {
      case '\n':
      case '\t': {
        *buf = ' ';
        break;
      }
      default: {
        // do nothing
      }
    }

    buf++;
  }

  return;
}

//-- Trims the trailing spaces by moving the null terminator backwards.
void trim_trailing_ws(char *buf) {
  
  int spaces_found = 0;

  char* ptr = buf;              // initialize

  while(*ptr) ptr++;            // find null terminator

  ptr--;                        // backup one position
  
  while(*ptr == ' ') {          // backup null pointer over the spaces
    ptr--;
    spaces_found = 1;
  }
  
  if (spaces_found) {           // place null terminator at the 
    *(ptr + 1) = '\0';          // first space
  }
  
  return;
}

//-- Remove spaces from the front of the line by shifting the string
//-- starting at the first non-space character.
void trim_leading_ws(char *buf) {
  
  char* ptr_from = buf;           // initialize source and destiation pointers
  char* ptr_to   = buf;
  
  if(*buf == ' ') {  // leading spaces found ---------------------------------

    while(*ptr_from == ' ') {    // find the start of the string
      ptr_from++;
    }
  
    while(*ptr_from != '\0') {   // copy everything but null terminator
      *ptr_to = *ptr_from;
      ptr_to++;
      ptr_from++;
    }

    *ptr_to = *ptr_from;         // copy null terminator
    ptr_to++;
    
    while(*ptr_from != '\0') {   // clear trailing items, probably not needed
      *ptr_to = ' ';
      ptr_to++;
    }
    *ptr_to = ' ';
  }

  return;
}

//-- Removes all extra-spaces by looking for repeated spaces and skippping
//-- over them.  This method modifies the passed string, but the string
//-- will never grow only shrink but removing excess characters.
void remove_duplicate_ws(char *buf) {
  
  char* ptr_from = buf;         // initialize source and destiation pointers
  char* ptr_to   = buf;
  
  int   copying  = 0;  // Flag is set when multiple spaces are detected
                       // and copying from one part of the string to another.

  while(*ptr_from != '\0') {
    
    if(copying) {                 /*** dup spaces detected, copying        ***/

      switch (*ptr_from) {
        case ' ': {

          *ptr_to = *ptr_from;      // copy first space then increment ptrs
          ptr_to++;
          ptr_from++;
          
          while(*ptr_from == ' ') { // find next non-space incrementing the
            ptr_from++;             // from pointer
          }
          break;
        }
        default: {
          *ptr_to = *ptr_from;      // all other characters are copied
          ptr_to++;
          ptr_from++;
          break;
        }
      }

    } else {                      /*** dup spaces not yet detected         ***/

      if(*ptr_from       == ' ' &&
         *(ptr_from + 1) == ' '    ) {  // dup spaces have been detected

        copying = 1;                    // flag set switching processing mode
        
        ptr_to = ptr_from + 1;          // initialize destination pointer
        
        while(*ptr_from == ' ') {       // move source ptr to next non-space
          ptr_from++;           
        }
        
        *ptr_to = *ptr_from;            // start to copy process
        ptr_to++;
        ptr_from++;

      } else {                    /*** no dup spaces, just iterate         ***/
        ptr_from++;
      }
    }
  }

  if(copying) {
    *ptr_to = *ptr_from;  // copy the null-terminator character, if copying
  }

  return;
}

//-- Accepts a string with command arguments, and splits them into 
//-- parts.  A character pointer array is created with an empty slot
//-- at index 0 for the command.  The char pointer array is returned
//-- as a double pointer char.
char **split_args_str(char *str) {
  
  int  cnt_spaces = 1;            // initialize for a single argument, 
                                  // there will be no associated space

  char *ptr       = str;          // initialize pointer

  while(*ptr) {                   // count spaces for additional arguments
    if(*ptr == ' ') cnt_spaces++;
    ptr++;
  }

  cnt_spaces += 2;                // add one slot for both the command and
                                  // terminating null

  // create array for each argument, the command a index 0, and a
  // terminating null pointer.
  char **ptr_array = malloc((cnt_spaces + 2) * sizeof(char **));
  
  ptr = str;                      // reset the point to front of str
  
  ptr_array[0] = NULL;            // initialize command slot as null and
  int index    = 1;               // start index at next slot

  ptr_array[index] = ptr;         // store the first argument
  index++;                        // index the next slot

  while(*ptr) {
    if(*ptr == ' ') {
      *ptr = '\0';                // terminate current argument
      ptr_array[index] = ptr + 1; // store next argument
      index++;                    // index the next slot
    }
    ptr++;
  }

  ptr_array[index] = NULL;        // store the terminating null

  return ptr_array;
}

void print_error(void) {
  char* msg = "An error has occurred\n";
  write(STDERR_FILENO, msg, strlen(msg));
}
