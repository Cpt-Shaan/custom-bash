/************
 * Custom Bash Shell Implementation
 * Prototype - 1.0
 * No support for pipelining provided
 * Support for certain keyboard signals not available
 */

/*
Initial notes - 
1. cd is not handled by execvp() syscall hence is done using chdir() and is handled by seperate conditionals.
2. To simulate an init process (parent process which keeps running as 'bash') an infinite loop has been chosen.
3. Signal handling for Ctrl-C and Ctrl-Z has been provided to replicate bash behavior of process termination.
*/

#include <stdio.h>      // standard library for i/o operations
#include <string.h>     // for string manipulation - strstep(), etc
#include <stdlib.h>     // exit() 
#include <unistd.h>     // fork(), getpid(), exec()
#include <sys/wait.h>   // wait()
#include <signal.h>     // signal()
#include <fcntl.h>      // close(), open()
#include <ctype.h>

// Since C only supports fixed sized arrays in statc allocation
#define MAX_PROCS 8     // max processes that can run parallely
#define MAX_ARGS 10     // max arguments per command including tags and options

// Function prototypes
void parseInput(char* input_str, char** args);
void executeCommand(char** args);
void executeParallelCommands(char* input_str);
void executeSequentialCommands(char* input_str);
void executeCommandRedirection(char* input_str);
char* trimStr(char* input_str);  // String utility function

// Parse the input string and seperate cmd, tags, options, args for execvp 
void parseInput(char* input_str, char** args){
    int i = 0;
    // seperates string on empty space
    while((args[i] = strsep(&input_str, " ")) != NULL){
        if(*args[i] != '\0'){   // token should not be a terminating char
            i++;
        }
    }
    // execvp needs last char to be a NULL to indicate end of args
    args[i] = NULL;
}

// Execute a single command with tags, options, args
// Takes an array for input to execvp()
void executeCommand(char** args){
    if(args[0] == NULL){
        return;
    }

    // Fork a child, whose image will be replaced by execvp()
    pid_t pid = fork();

    if(pid == -1){
        // fork() failed
        printf("Shell: Incorrect command\n");
        return;
    }
    else if(pid == 0){
        // child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        if(execvp(args[0], args) < 0){
            // if returns an error occured
            printf("Shell: Incorrect command\n");
            exit(EXIT_FAILURE); // child exited
        }
    }
    else{
        // parent process
        // wait(NULL);
        int status;
        waitpid(pid, &status, WUNTRACED);
    }
}

// Execute multiple commands with tags, options, args in parallel (limit of MAX_PROCS)
void executeParallelCommands(char* input_str){
    char* commands[MAX_PROCS];
    int i = 0;

    // Split commands by "&&"
    while((commands[i] = strsep(&input_str, "&&")) != NULL && i < MAX_PROCS){
        commands[i] = trimStr(commands[i]);
        if(*commands[i] != '\0'){
            i++;
        }
    }

    int num = i;
    pid_t pids[num];

    // for each process fork() a child process
    for(i=0 ; i<num ; i++){
        char* args[MAX_ARGS];
        parseInput(commands[i], args);

        if(args[0] != NULL){
            pids[i] = fork(); 

            if(pids[i] < 0){
                printf("Shell: Incorrect command\n");
                return;
            }
            else if(pids[i] == 0){
                // Child Process
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                if(execvp(args[0], args) < 0){
                    printf("Shell: Incorrect command\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // parent must wait for all child processes to complete
    for(i = 0 ; i<num ; i++){
        waitpid(pids[i], NULL, 0);
    }
}

// Execute multiple commands with tags, options, args sequentially  (no limits specified)
void executeSequentialCommands(char* input_str){
    char* command;

    // split input on "##"
    while((command = strsep(&input_str, "##")) != NULL){
        command = trimStr(command);
        if(*command != '\0'){
            char* args[MAX_ARGS];
            parseInput(command, args);

            // cd being handled seperately
            if(args[0] != NULL && strcmp(args[0], "cd") == 0){
                if(args[1] == NULL){    // if no dir specified after cd
                    printf("Shell: Incorrect command\n");
                }
                else{
                    if(chdir(args[1]) != 0){
                        printf("Shell: Incorrect command\n");
                    }
                }
            }
            else{
                executeCommand(args);
            }
        }
    }
}

// Executes a single command having tags, options, args with its output redirected to a file
void executeCommandRedirection(char* input_str){
    char* command;
    char* filename; 

    // Split input on ">" character
    command = strsep(&input_str, ">");
    filename = input_str;

    if(command == NULL || filename == NULL){
        printf("Shell: Incorrect command\n");
        return;
    }

    command = trimStr(command);
    filename = trimStr(filename);

    if(*command == '\0' || *filename == '\0'){
        printf("Shell: Incorrect command\n");
        return;
    }

    char* args[MAX_ARGS];
    parseInput(command, args);

    // Forking a child process
    pid_t pid = fork();

    if(pid < 0){
        printf("Shell: Incorrect command\n");
        return;
    }
    else if(pid == 0){
        // Child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        // open file for write only, create if it doesnt exist, and truncate it
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if(fd < 0){
            printf("Shell: Incorrect command\n");
            exit(EXIT_FAILURE);
        }

        // redirect standard output to file
        dup2(fd, STDOUT_FILENO);
        close(fd);

        if(execvp(args[0], args) < 0){
            printf("Shell: Incorrect command\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        int status;
        waitpid(pid, &status, WUNTRACED);
    }
}

// Utility function to remove trailing and leading white spaces
char* trimStr(char* input_str){
    char* end_pos;

    while(isspace((unsigned char)*input_str)) {
        input_str++;
    }

    if(*input_str == 0){
        return input_str;
    }

    end_pos = input_str + strlen(input_str) - 1;
    while(end_pos > input_str && isspace((unsigned char)*end_pos)){
        end_pos--;
    }
    end_pos[1] = '\0';

    return input_str;
}

int main(){

    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char cwd[1024];

    // Signal Handling (Ctrl+C and Ctrl+Z)
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Infinite while loop - runs till exit cmd. Simulates init process
    while(1){

        // input prompt 'cwd$' - current working directory
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("%s$ ", cwd);
        }
        else{
            perror("getcwd() error");
            break;
        }
        
        // read a line from the terminal
        read = getline(&line, &len, stdin);

        // Ctrl-D exit
        if(read == -1){
            printf("Exiting shell...\n");
            break;
        }

        // Remove trailing newline character
        if (line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        // If the command is empty, just show the prompt again
        if (strlen(trimStr(line)) == 0) {
            continue;
        }

        char* line_copy = strdup(line); // Create a copy for parsing

        // Parse input to check for built-in commands first
        char* args[MAX_ARGS];
        parseInput(line_copy, args);
        
        // Check for "exit" command
        if (args[0] != NULL && strcmp(args[0], "exit") == 0) {
            printf("Exiting shell...\n");
            free(line_copy);
            break;
        }

        // Check for special operators and call the appropriate function
        if (strstr(line, "&&") != NULL) {
            executeParallelCommands(line);
        } 
        else if (strstr(line, "##") != NULL) {
            executeSequentialCommands(line);
        } 
        else if (strstr(line, ">") != NULL) {
            executeCommandRedirection(line);
        } 
        else {
            // seperate case to handle cd
            if (args[0] != NULL && strcmp(args[0], "cd") == 0) {
                if (args[1] == NULL) {
                    printf("Shell: Incorrect command\n");
                } 
                else {
                    if (chdir(args[1]) != 0) {
                        printf("Shell: Incorrect command\n"); 
                    }
                }
                free(line_copy);
                continue; 
            }
            else{
                parseInput(line, args);
                executeCommand(args); // when user wants to run a single command
            }
        }

        free(line_copy);
    }

    free(line); // free memory allocated by getline()
    return 0;
}
