#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// define global variables
#define MAX_LENGTH 2048
#define MAX_ARGS 512
bool is_it_foreground = false;
bool background_allowed = true;
int fg_status; // foreground status
// #define CONT_STATUS 1

/* Our signal handler for SIGINT */
void handle_SIGINT(int signo){
	char* message = "Caught SIGINT, sleeping for 10 seconds\n";
	// We are using write rather than printf
	write(STDOUT_FILENO, message, 39);
	// sleep(10);
}

void handle_SIGTSTP(int signo) {
    // entering foreground only mode
    if (background_allowed == true) { // if background mode is allowed
        // printf("Entering foreground-only mode (& is now ignored)\n");
        char* fg_msg1 = "Entering foreground-only mode (& is now ignored)\n";
        int msg1_len = strlen(fg_msg1);
        write(1, fg_msg1, msg1_len);
        fflush(stdout);
        background_allowed = false;
    }
    // exiting foreground only mode
    else {
        // printf("Exiting foreground-only mode (& is not ignored\n");
        char* fg_msg2 = "Exiting foreground-only mode (& is not ignored)\n";
        int msg2_len = strlen(fg_msg2);
        write(1, fg_msg2, msg2_len);
        fflush(stdout);
        background_allowed = true;
    }
}

void get_user_input(char* user_input[], int pid, char i_file[], char o_file[], int* bg_status);

void handle_exec(char* user_input[], int* exit_status, struct sigaction SIGINT_action, int* bg_status, char i_file[], char o_file[]);

int main() {
    
    int cont_status = 1;
    int status_signal;
    int pid = getpid();
    int exit_status = 0;
    int bg_status = 0; // bg_status = 1 if program has background process

    // terminate signal (SIGINT)
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handle_SIGINT;
    // block all catchable signals while sigint is running
    sigfillset(&SIGINT_action.sa_mask);
    // no flags set
    SIGINT_action.sa_flags = 0;

    // Install our signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

    // struct sigaction terminate_signal;
    // terminate_signal.sa_handler = term_sig;

    // signal SIGTSTOP
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    char* user_input[MAX_ARGS];
    char i_file[MAX_LENGTH];
    char o_file[MAX_LENGTH];
    memset(user_input, '\0', sizeof(user_input));

    while (cont_status == 1) {
        // printf("at the top\n");
        // get input
        
        // printf(": ");
        // // fflush(stdin);
        // fflush(stdout);

        get_user_input(user_input, pid, i_file, o_file, &bg_status);
        // test for exit
        if (strcmp(user_input[0], "exit") == 0) {
            // break the while loop
            cont_status = 0;
        }
        // test for comment or blank
        else if (user_input[0][0] == '#' || user_input[0][0] == '\0') {
            // don't break the while loop
            cont_status = 1;
        }
        // test for cd
        else if (strcmp(user_input[0], "cd") == 0) {
            // change directories
            if (user_input[1] != '\0') {
                // if chdir() returns -1, it is not successful
                if (chdir(user_input[1]) == -1) {
                    printf("no such file or directory.\n");
                    fflush(stdout);
                }
            }
            // if the new directory isn't provided
            else {
                // change to home directory
                chdir(getenv("HOME"));
            }
        }
        // check status
        else if (strcmp(user_input[0], "status") == 0) {
            // https://www.geeksforgeeks.org/exit-status-child-process-linux/ was referenced
            if (WIFEXITED(exit_status)) {
                int temp_status = WEXITSTATUS(exit_status);
                printf("exit value %d\n", temp_status);
            }
            // if it was not terminated by a status
            else {
                // https://www.ibm.com/docs/en/ztpf/2019?topic=zca-wtermsig-determine-which-signal-caused-child-process-exit
                int temp_status2 = WTERMSIG(exit_status);
                printf("terminated by signal %d\n", temp_status2);
            }
        }
        // exec command
        else {
            handle_exec(user_input, &exit_status, SIGINT_action, &bg_status, i_file, o_file);
        }

        // printf("here\n");

        // reset everything before it loops again
        memset(user_input, '\0', sizeof(user_input));
        bg_status = 0;
        i_file[0] = '\0';
        o_file[0] = '\0';

        // printf("at the bottom\n");

    }
    

    return 0;
}

void get_user_input(char* user_input[], int pid, char i_file[], char o_file[], int* bg_status) {

    char temp_arr[MAX_LENGTH];
    int temp_val = 0;

    printf(": ");
    // fflush(stdin);
    fflush(stdout);

    // read input from stdin
    fgets(temp_arr, MAX_LENGTH, stdin);

    // replace new line character with NULL
    for(int i = 0; (i < MAX_LENGTH && temp_val == 0); ++i) {
        // if the character is a new line character
        if (temp_arr[i] == '\n') {
            // replace it with a null character
            temp_arr[i] = '\0';
            // change value of temp_val to stop for loop
            temp_val = 1;
        }
    }

    // check if the input is blank
    if (strcmp(temp_arr, "") == false) {
        user_input[0] = "";
        exit(1);
    }

    // separate user input by spaces
    char* str_token = strtok(temp_arr, " ");

    // loop through token to check every argument in the command
    for (int i = 0; str_token != NULL; ++i) {

        // check for input file
        if (strcmp(str_token, "<") == 0) {
            // get the file name after the "<" and space
            str_token = strtok(NULL, " ");
            strcpy(i_file, str_token);
        }
        // check for output file
        else if (strcmp(str_token, ">") == 0) {
            // get the file name after the ">" and space
            str_token = strtok(NULL, " ");
            strcpy(o_file, str_token);
        }
        // check if it should be a background process
        else if (strcmp(str_token, "&") == 0) {
            *bg_status = 1;
        }
        // check if it is just an argument without a command
        else {
            // store the command to the user input array
            user_input[i] = strdup(str_token);

            // expand the $$ variable
            for (int x = 0; user_input[i][x]; ++x) {
                int y = x + 1;
                if (user_input[i][x] == '$' && user_input[i][y] == '$') {
                    user_input[i][x] = '\0';
                    snprintf(user_input[i], 256, "%s%d", user_input[i], pid); 
                }
            }
        }

        // move onto next one
        str_token = strtok(NULL, " ");
    }

}

void handle_exec(char* user_input[], int* exit_status, struct sigaction SIGINT_action, int* bg_status, char i_file[], char o_file[]) {
    // code & content from canvas modules are used
    
    // int in, out, results;
    pid_t spawnpid = -5; //!!! find out why this works

    // fork child process
    spawnpid = fork();
    switch(spawnpid) {

        case -1:
            // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
            perror("fork() failed!\n");
            exit(1);
            break;

        case 0:
            // spawnpid is 0. This means the child will execute the code in this branch
            SIGINT_action.sa_handler = SIG_DFL; // default action for signal
            // handle the SIGINT signal
            sigaction(SIGINT, &SIGINT_action, NULL);

            // Handle input
            if (strcmp(i_file, "")) {
                // open file
                int temp_in = open(i_file, O_RDONLY);
                if (temp_in == -1) {
                    perror("cannot open file for input\n");
                    exit(1);
                }
                // assign input file
                int temp_two = dup2(temp_in, 0);
                if (temp_two == -1) {
                    perror("cannot assign input file\n");
                    exit(2);
                }
                // close
                fcntl(temp_in, F_SETFD, FD_CLOEXEC);
            }

            // Handle output
            if (strcmp(o_file, "")) {
                // open file
                int temp_out = open(o_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
                // ^ review the 0666
                if (temp_out == -1) { // if it failed to open
                    perror("cannot open file for output\n");
                    exit(1);
                }
                // assign output file
                int temp_two = dup2(temp_out, 1);
                if (temp_two == -1) { // if it failed to assign
                    perror("cannot assign output file\n");
                    exit(2);
                }
                // close
                fcntl(temp_out, F_SETFD, FD_CLOEXEC);
            }

            int test = execvp(user_input[0], user_input);

            if (test == -1) {
                printf("%s: no such file or directory\n", user_input[0]);
                fflush(stdout);
                exit(2);
            }
            break;

        default:
            // spawnpid is the pid of the child. This means the parent will execute the code in this branch
            // MAKE SURE BACKGROUND MODE IS TRUE!!!
            // https://replit.com/@cs344/42waitpidnohangc?v=1#main.c is referenced
            if (background_allowed == true && *bg_status == 1) {
                pid_t childpid = waitpid(spawnpid, exit_status, WNOHANG);
                //printf("background pid is %d", spawnpid);
                fflush(stdout);
            }
            else { // if background mode isn't authorized, do this
                pid_t childpid = waitpid(spawnpid, exit_status, 0);
            }
        
        // check if there are any bg processes that terminated
        // spawnpid = waitpid(-1, exit_status, WNOHANG);
        // if (spawnpid > 0) {
        while ((spawnpid = waitpid(-1, exit_status, WNOHANG)) > 0) {
            printf("child process %d terminated.\n", spawnpid);
            
            // https://www.geeksforgeeks.org/exit-status-child-process-linux/ was referenced
            if (WIFEXITED(exit_status)) {
                int temp_status = WEXITSTATUS(exit_status);
                printf("exit value %d\n", temp_status);
            }
            // if it was not terminated by a status
            else {
                // https://www.ibm.com/docs/en/ztpf/2019?topic=zca-wtermsig-determine-which-signal-caused-child-process-exit
                int temp_status2 = WTERMSIG(exit_status);
                printf("terminated by signal %d\n", temp_status2);
            }
            
            fflush(stdout);
        }
    }


}