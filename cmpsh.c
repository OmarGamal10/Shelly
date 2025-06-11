
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include <glob.h>
#include<readline/readline.h>
#include<readline/history.h>
#include <sys/wait.h>

#define MAX_COMMANDS 10



void trim(char *line){
  char *start = line;
  while(*start == ' ' || *start == '\t')
    start++;
  char *end = start + strlen(start) - 1;
  while(end > start && (*end == ' ' || *end == '\t'))
    end--;
  *(end + 1) = '\0';
  memmove(line, start, strlen(start) + 1);
}

void cd(char *path) {
  if(path == NULL) {
    chdir(getenv("HOME"));
  }
  else {
    while(*path == ' ' || *path == '\t')
      path++;
    if(chdir(path) == -1) {
      perror("chdir");
    }
  }
}

void pwd(char *output_file) {
  //be aware this is different than child processes, i have to restore stdout as this is the main process XD
  int stdout_copy = -1;
  if(output_file != NULL) {
    stdout_copy = dup(STDOUT_FILENO); // save the original stdout
    FILE *file = fopen(output_file, "w");
    if(file == NULL) {
      perror("could not open file");
      return;
    }
    dup2(fileno(file), STDOUT_FILENO);
    fclose(file); //standard output points to file
  }
  char *cwd;
  if((cwd = getcwd(NULL, 0)) != NULL) {
    printf("%s\n", cwd);
    free(cwd);
  }
  else {
    perror("getcwd");
  }
  if(stdout_copy != -1){
    fflush(stdout);
    dup2(stdout_copy, STDOUT_FILENO); // restore stdout
    close(stdout_copy); // close the copy
  }
}

void handle_path(char **paths, char *arg_paths) {
  for(int i = 0; i < 50; ++i) {
    if(paths[i] == NULL)
      break;
    free(paths[i]);
    paths[i] = NULL;
  }
  if(arg_paths == NULL || *arg_paths == '\0') {
    paths[0] = strdup("/bin");
    paths[1] = NULL;
    return;
  }
  char *path_dup = strdup(arg_paths);
  if(path_dup == NULL) { 
    paths[0] = strdup("/bin");
    paths[1] = NULL;
    return;
  }
  int i = 0;
  char *token = strtok(path_dup, " ");
  while(token != NULL && i < 49) { 
    paths[i] = strdup(token);
    i++;
    token = strtok(NULL, " ");
  }
  paths[i] = NULL;
  
  free(path_dup); 
}

// Finds the path of the executable of an external program defined via a command
char *find_executable(char **paths, char *cmd){
  for(int i = 0; paths[i] != NULL; ++i){
    char *path = malloc((strlen(paths[i]) + strlen(cmd) + 2) * sizeof(char));
    sprintf(path, "%s/%s", paths[i], cmd);
    if(access(path, X_OK) == 0){
      return path;
    }
    free(path);
  }
  return NULL;
}

void signal_handler(){
  printf("\n");
  fflush(stdout);
  signal(SIGINT, signal_handler);
  signal(SIGTSTP, signal_handler);
}


void execute_command(char **paths, char *cmd, char* args, char *output_file){
  char *executable_path = find_executable(paths, cmd);
  if(executable_path == NULL){
    printf("Command not found: %s\n", cmd);
    return;
  }
  pid_t pid = fork();
  if(pid == 0){
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    if(output_file != NULL){
      FILE *file = fopen(output_file, "w");
      if(file == NULL){
        perror("could not open file");
        exit(1);
      }
      dup2(fileno(file), STDOUT_FILENO);
      fclose(file); //standard output points to file
    }
    char *argv[15]; 
    argv[0] = strdup(cmd);
    if(args == NULL){
      argv[1] = NULL;
    }
    else{
      char *args_dup = strdup(args);
      char *arg;
      int i = 1;
      arg = strtok(args_dup, " ");
      while(arg != NULL){
        if(strchr(arg, '*') != NULL){  //little bit of perfectionism sorry
          glob_t globbuf;
          if(glob(arg, GLOB_NOCHECK, NULL, &globbuf) == 0){
            for(size_t j = 0; j < globbuf.gl_pathc; ++j){
              argv[i] = strdup(globbuf.gl_pathv[j]);
              //printf("arg: %s\n", argv[i]);
              ++i;
            }
            globfree(&globbuf);
          }
        }
        else{
          argv[i] = strdup(arg); 
          //printf("arg: %s\n", argv[i]);
          ++i;
        }
        arg = strtok(NULL, " ");
      }
      argv[i] = NULL;
      free(args_dup);
    }
    execv(executable_path, argv);
  }
  else if(pid < 0){
    perror("fork");
  }
  else{
    int status;
    waitpid(pid, &status, 0);
    free(executable_path);
    if(WIFEXITED(status)){
      //printf("Child process exited with status %d\n", WEXITSTATUS(status));
    }
    else{
      //printf("Child process terminated abnormally\n");
    }
  }
}

int parse_pipeline(char *line, char **commands) {
  int count = 0;
  int in_quotes = 0;
  char *start = line;
  
  for (char *p = line; *p; p++) {
      if (*p == '"') {
          in_quotes = !in_quotes;
      } else if (*p == '|' && !in_quotes) {
          *p = '\0';
          commands[count++] = start;
          start = p + 1;
          while (*start == ' ' || *start == '\t')
              start++;
      }
  }
  commands[count++] = start;
  return count;
}

void execute_pipeline(char **paths, char **commands, int cmd_count, char *output_file) {
  int pipes[MAX_COMMANDS-1][2];
  pid_t pids[MAX_COMMANDS];
  
  for (int i = 0; i < cmd_count - 1; i++) {
      if (pipe(pipes[i]) == -1) {
          perror("pipe");
          return;
      }
  }
  
  for (int i = 0; i < cmd_count; i++) {
      trim(commands[i]);
      char *cmd_copy = strdup(commands[i]);
      char *cmd = strtok(cmd_copy, " ");
      char *args = strtok(NULL, "");
      
      pids[i] = fork();
      if (pids[i] == 0) {
          // Child process
          signal(SIGINT, SIG_DFL);
          signal(SIGTSTP, SIG_DFL);
          
          // input from previous command IF NOT first command
          if (i > 0) {
              dup2(pipes[i-1][0], STDIN_FILENO);
          }
          
          // output to next command IF NOT last command :)
          if (i < cmd_count - 1) {
              dup2(pipes[i][1], STDOUT_FILENO);
          } else if (output_file != NULL) {
              // redirect output to file if specified (for last command ofc)
              FILE *file = fopen(output_file, "w");
              if (file == NULL) {
                  perror("could not open file");
                  exit(1);
              }
              dup2(fileno(file), STDOUT_FILENO);
              fclose(file);
          }
          
          // close pipes, we already have references to them
          for (int j = 0; j < cmd_count - 1; j++) {
              close(pipes[j][0]);
              close(pipes[j][1]);
          }
          
          char *executable_path = find_executable(paths, cmd);
          if (executable_path == NULL) {
              fprintf(stderr, "Command not found: %s\n", cmd);
              exit(1);
          }
          
          char *argv[15];
          argv[0] = strdup(cmd);
          if (args == NULL) {
              argv[1] = NULL;
          } else {
            // parse args while handling quotes (most painful part tbh)
              char *args_dup = strdup(args);
              int i = 1;
              int in_double_quotes = 0;
              int in_single_quotes = 0;
              char *arg_start = args_dup;
              char *p_write = args_dup;

              for (char *p = args_dup; ; p++) {
                if (*p == '\\' && *(p+1) != '\0' && !in_single_quotes) {
                  // Handle escape sequences
                  p++;
                  switch (*p) {
                    case 'n': *p_write++ = '\n'; break;
                    case 't': *p_write++ = '\t'; break;
                    case 'r': *p_write++ = '\r'; break;
                    case '\\': *p_write++ = '\\'; break;
                    case '\'': *p_write++ = '\''; break;
                    case '"': *p_write++ = '"'; break;
                    default: *p_write++ = *p; break;
                  }
                } else if (*p == '"' && !in_single_quotes) {
                  in_double_quotes = !in_double_quotes;
                } else if (*p == '\'' && !in_double_quotes) {
                  in_single_quotes = !in_single_quotes;
                } else if ((*p == ' ' && !in_double_quotes && !in_single_quotes) || *p == '\0') {
                  *p_write = '\0';

                  if (p_write > arg_start) {
                    argv[i++] = strdup(arg_start);
                  }

                  if (*p == '\0') break;
                  p_write++;
                  arg_start = p_write;
                } else {
                  *p_write++ = *p;
                }
              }
              argv[i] = NULL;
              free(args_dup);
          }
          execv(executable_path, argv);
          perror("execv");
          exit(1);
      } else if (pids[i] < 0) {
          perror("fork");
      }
      free(cmd_copy);
  }
  
  // Close all pipe fds in the parent
  for (int i = 0; i < cmd_count - 1; i++) {
      close(pipes[i][0]);
      close(pipes[i][1]);
  }
  
  // Wait for all children
  for (int i = 0; i < cmd_count; i++) {
      waitpid(pids[i], NULL, 0);
  }
}


int main(int argc, char *argv[]) {
  char **paths = malloc(sizeof(char*) * 50);
  paths[0] = strdup("/bin");
  char *line;
  int interactive = 1;
  FILE *input = NULL;
  if(argc > 1){
    interactive = 0;
    input = fopen(argv[1], "r");
    if(input == NULL){
      perror("fopen");
      return 1;
    }
  }
  if(interactive == 1){
    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
  }
  
  while(1) {
    if(interactive == 1){
      line = readline("shelly> ");
      if(line == NULL){
        break;
      }
    }
    else{
      line = malloc(512 * sizeof(char));
      if(fgets(line, 512, input) == NULL){
        free(line);
        line = NULL;
        break;
      }
      int len = strlen(line);
      if(len > 0 && line[len - 1] == '\n'){
        line[len - 1] = '\0';
      }
    }
    trim(line);
    char *original_line = strdup(line);
    
    // Check for pipe before checking for redirection
    char *pipe_pos = strchr(line, '|');
    char *redirect_pos = strchr(line, '>');
    char *output_file = NULL;
    
    // Handle output redirection
    if (redirect_pos != NULL) {
        *redirect_pos = '\0';
        output_file = strtok(redirect_pos + 1, " ");
        trim(output_file);
    }
    
    if (pipe_pos != NULL) {
        // Handle pipeline
        char *commands[MAX_COMMANDS];
        int cmd_count = parse_pipeline(line, commands);
        execute_pipeline(paths, commands, cmd_count, output_file);
    } else {
        // Handle single command as before
        char *cmd = strtok(line, " ");
        if (cmd == NULL) {
            free(line);
            free(original_line);
            line = NULL;
            original_line = NULL;
            continue;
        }
        if (strcmp(cmd, "exit") == 0) {
            free(line);
            free(original_line);
            line = NULL;
            original_line = NULL;
            break;
        }
        if (strcmp(cmd, "cd") == 0) {
            char *path = strtok(NULL, "");
            cd(path);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            pwd(output_file);
        }
        else if (strcmp(cmd, "path") == 0) {
            char *path = strtok(NULL, "");
            if (path == NULL) {
                path = "";
            }
            handle_path(paths, path);
        }
        else {
            // External program
            char *args = strtok(NULL, "");
            execute_command(paths, cmd, args, output_file);
        }
    }
    
    if (interactive && line[0] != '\n') {
        add_history(original_line);
    }
    free(line);
    free(original_line);
    line = NULL;
    original_line = NULL;
  }

  if(interactive == 0){
    fclose(input);
  }
  


  for(int i = 0; i < 50; ++i) {
    if(paths[i] != NULL) {
      free(paths[i]);
    }
  }
  if(paths != NULL) {
    free(paths);
  }
  return 0;
}
