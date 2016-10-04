#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/mman.h>
#include <readline/readline.h>
#include <readline/history.h>

const int bufferSize = 1000;
const int sleepTime = 10000;
int *failed; //Pointer for communicating between forks.

typedef struct node {
    int pid;
    int isRunning;
    char* cmd;
    struct node *next;
} node;

node* head = NULL;

//Utility function for adding a node to the linked list.
void addNode(int pid, char* cmd, int isRunning) {
    node* newNode = malloc (sizeof(node));
    newNode->pid = pid;
    newNode->isRunning = isRunning;
    newNode->next = NULL;
    newNode->cmd = strdup(cmd);

    if (head == NULL) {
        //empty linkedlist
        head = newNode;
        return;
    }
    node *curr = head;
    while(curr->next != NULL) {
        curr = curr->next;
        //traverse
    }
    curr->next = newNode; //Set new node

}

//Utility function for removing a node from the linked list.
void removeNode(int pid) {
    if (head == NULL) return; //Nolist case
    node* prev = NULL;
    node* curr = head;

    while(curr == NULL || curr->pid != pid) {
        if (curr == NULL) {
            //pid not in linkedlist
            return;
        }
        prev = curr; //iter
        curr = curr->next; //iter
    }

    if (curr == head) {
        //firstElem
        head = curr->next;
    }
    if (prev != NULL) {
        //Not first element
        prev->next = curr->next;
    }
}


//Utility function for finding a node in the linked list.
node* findNode(int pid) {
    node* curr = head;
    while (curr == NULL || curr->pid != pid) {
        if (curr == NULL) {
            //Not in list
            break;
        }
        curr = curr->next;
    }
    return curr;
}

int processExists(pid) {
    char dirBuffer[bufferSize];
    sprintf(dirBuffer, "/proc/%d", pid);
    DIR* dir = opendir(dirBuffer);

    if (!dir) {
        //Directory does not exist, and therefore the process.
        if (ENOENT == errno) {
            printf("Process %d does not exist.\n", pid);
        } else {
            printf("Process %d is unaccessible.\n", pid);
        }
        return 0;
    }
    return 1;
}

//Utility function for stoppping a process.
void stopProcess(int pid) {
    if (!processExists()) return;
    if (kill(pid, SIGSTOP)) {
        printf("Failed to stop process %d\n", pid);
    }
}

//Utility function for starting a process.
void startProcess(int pid) {
    if (!processExists()) return;
    if (kill(pid, SIGCONT)) {
        printf("Failed to start process %d\n", pid);
    }
}

//Utility function for killing a process.
void killProcess(int pid) {
    if (!processExists()) return;
    node* myNode = findNode(pid);
    if (myNode && myNode->isRunning == 0) {
        printf("Restarting process %d to safekill it\n", pid);
        startProcess(pid);
    }
    usleep(sleepTime);
    if (kill(pid, SIGTERM)) {
        printf("Failed to terminate process %d\n", pid);
    }
}

//Prints the linked list
void printLinkedList() {
    node* curr = head;
    int numProcesses = 0;
    while(curr != NULL) {
        numProcesses++;
        printf("%d: %s %s\n", curr->pid, curr->cmd, curr->isRunning ? "Running" : "Stopped");
        curr = curr->next;
    }
    printf("Total Number of Processes: %d\n", numProcesses);

}

//Called to update the background process, cleaning up or adding to the linked list.
void updateBackgroundProcess() {
    int status;
    int pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED);
    if (pid > 0) {
        if (WIFCONTINUED(status)) {
            //Continued
            //status 65535
            node* myNode = findNode(pid);
            if (myNode) {
                myNode->isRunning = 1;
            }
            printf("%d : Started\n", pid);
        } else if (WIFSTOPPED(status)) {
            //Stopped
            //status 4991
            node* myNode = findNode(pid);
            if (myNode) {
                myNode->isRunning = 0;
            }
            printf("%d : Stopped\n", pid);
        } else if (WIFEXITED(status)) {
            //Finished
            //status 256
            removeNode(pid);
            printf("%d : Finished\n", pid);
            //Remove it from the linkedList
        } else if (WIFSIGNALED(status)) {
            //Killed (RIP Harambe)
            //status 9
            removeNode(pid);
            printf("%d : Terminated\n", pid);
            //Remove it from the linkedList
        }
    }
}

//split on a given token and pass the result back through result
void splitOn(char* token, char contents[], char* result[]) {
    char* iterToken = strtok(contents, token);
    int i = 0;
    while (iterToken != NULL) {
        result[i] = strdup(iterToken);
        iterToken = strtok(NULL, token);
        i++;
    }
}

//implementation of pstat
void printStat(int pid) {
    FILE* statfp;
    FILE* statusfp;
    char line[bufferSize];
    char* stat[bufferSize];
    char status[50][bufferSize];
    char* token = " ";
    char statBuffer[bufferSize];
    char statusBuffer[bufferSize];
    long int nonvoluntary, voluntary;
    int i = 0;
    int j = 0;

    if (!processExists(pid)) return;
    //status
    sprintf(statusBuffer, "/proc/%d/status", pid);
    statusfp = fopen(statusBuffer, "r");

    while (fgets(status[j], 256, statusfp) != NULL) {
        j++;
    }

    sscanf(status[j - 2], "voluntary_ctxt_switches: %ld", &voluntary);
    sscanf(status[j - 1], "nonvoluntary_ctxt_switches: %ld", &nonvoluntary);

    //stat
    sprintf(statBuffer, "/proc/%d/stat", pid);
    statfp = fopen(statBuffer, "r");

    if (statfp == NULL) {
        printf("Could not open proc for pid %d", pid);
    }

    fgets(line, sizeof(line), statfp) != NULL;

    splitOn(token, line, stat);

    fclose(statfp);
    fclose(statusfp);

    printf("pstat for pid %s\n", stat[0]);
    printf("comm: %s\n", stat[1]);
    printf("state: %s\n", stat[2]);
    printf("utime: %ld\n", atoi(stat[13])/sysconf(_SC_CLK_TCK));
    printf("stime: %ld\n", atoi(stat[14])/sysconf(_SC_CLK_TCK));
    printf("rss: %s\n", stat[23]);
    printf("voluntary_ctxt_switches: %ld\n", voluntary);
    printf("nonvoluntary_ctxt_switches: %ld\n", nonvoluntary);
}

//Verify that command input meets usage spec.
int verifyInput(char input[]) {
    if (!input) {
        printf("No job specified\n");
        return 0;
    } else if (!isDigit(input)) {
        printf("Pid must be an integer\n");
        return 0;
    } else {
        return 1;
    }
}

//Utility function for determining if a string is a digit.
int isDigit(char input[]) {
    int length = strlen(input);
    int i;
    for (i = 0; i < length; i++) {
        if (!isdigit(input[i])) {
            return 0;
        }
    }
    return 1;
}

//Function for processing the user input.
void processInput(char* args[], int arglength) {
    int cp;
    char str[bufferSize];
    if (strcmp(args[0], "exit") == 0) {
        //exit
        printf("Exiting\n");
        exit(1);
    } else  if (strcmp(args[0], "bg") == 0) {
        //bg <cmd>
        if (!args[1]) {
            printf("No job specified\n");
        } else {
            int id = fork();
            strcpy(str, "");
            *failed = 0; //set fork communication to not failed
            for (cp = 1; cp < arglength; cp++) {
                strcat(str, args[cp]);
                strcat(str, " ");
            }
            if ( id == 0 ) {
                //Child Process
                execvp(args[1], &args[1]);
                printf("Failed to perform action %s\n", str);
                *failed = 1; //set fork failed flag to be read in parent
                exit(1); //exit so we don't have two pmen
            } else if ( id > 0) {
                //Parent Process
                usleep(sleepTime);
                if (*failed == 0) {
                    //if the child failed to load
                    printf("Process created with id %d\n", id);
                    addNode(id, str, 1);
                }
                usleep(sleepTime);
            } else {
                //forking failed, join the dark side.
                printf("Forking Failed");
            }
        }
    } else if (strcmp(args[0], "bglist") == 0) {
        //bglist <pid>
        printLinkedList();
    } else if (strcmp(args[0], "bgkill") == 0) {
        //bgkill <pid>
        if (verifyInput(args[1])) {
            killProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "bgstop") == 0) {
        //bgstop <pid>
        if (verifyInput(args[1])) {
            stopProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "bgstart") == 0) {
        //bgstart <pid>
        if (verifyInput(args[1])) {
            startProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "pstat") == 0) {
        //pstat <pid>
        if (verifyInput(args[1])) {
            printStat(atoi(args[1]));
        }
    } else {
        printf("%s: command not found\n", args[0]);
    }
}

int main(){
    int i,j;
    char *token = " ";
    char *prompt = "your command: ";
    const int maxArgs = 100;
    failed = mmap(NULL, sizeof *failed, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    while (1) {
        i = 0; //reset i so you can write over args
        char *input = NULL;
        char *iterToken;
        char *args[maxArgs];


        updateBackgroundProcess(); //update bglist
        input = readline(prompt);
        printf("\n");

        if (!strcmp(input,"")) {
            //invalid input
            continue;
        }

        iterToken = strtok(input, token);
        for (j = 0; j < maxArgs; j++) {
            if (iterToken) i++; //track how many args
            args[j] = iterToken; //Grab tokens (or nulls to clear)
            iterToken = strtok(NULL, token); //nextToken
        }

        processInput(args, i);
        usleep(sleepTime);
        updateBackgroundProcess(); //update bglist again
        printf("\n");
    }
}
