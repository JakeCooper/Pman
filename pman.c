#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

const int bufferSize = 1000;

typedef struct node {
    int pid;
    int isRunning;
    char* cmd;
    struct node *next;
} node;

node* head = NULL;

void KillProcess(int pid) {
    if (kill(pid, SIGKILL)) {
        printf("Failed to kill process %d\n", pid);
    } else {
        printf("Successfully killed process %d\n", pid);
    }
}

void StopProcess(int pid) {
    if (kill(pid, SIGSTOP)) {
        printf("Failed to stop process %d\n", pid);
    } else {
        printf("Successfully stopped process %d\n", pid);
    }
}

void StartProcess(int pid) {
    if (kill(pid, SIGCONT)) {
        printf("Failed to start process %d\n", pid);
    } else {
        printf("Successfully started process %d\n", pid);
    }
}


void AddNode(int pid, char* cmd, int isRunning) {
    node* newNode = malloc (sizeof(node));
    newNode->pid = pid;
    newNode->isRunning = isRunning;
    newNode->next = NULL;
    newNode->cmd = strdup(cmd);

    if (head == NULL) {
        head = newNode;
        return;
    }
    node *curr = head;
    while(curr->next != NULL) curr = curr->next;
    curr->next = newNode;

}

void RemoveNode(int pid) {
    if (head == NULL) return;
    node* prev = NULL;
    node* cur = head;

    while(cur->pid != pid) {
        if (cur->next == NULL) {
            printf("pid %d does not exist in Pman.\n", pid);
            return;
        }
        prev = cur->next;
        cur = cur->next;
    }

    if (cur == head) head = cur->next;
    if (prev != NULL) prev->next = cur->next;
    free(cur);
}

node* FindNode(int pid) {
    node* curr = head;
    while (curr == NULL || curr->pid != pid) {
        if (curr == NULL) {
            printf("pid %d does not exist or was not started by Pman\n", pid);
            break;
        }
        curr = curr->next;
    }
    return curr;
}

void PrintLinkedList() {
    node* curr = head;
    int numProcesses = 0;
    while(curr != NULL) {
        numProcesses++;
        printf("%d: %s %s\n", curr->pid, curr->cmd, curr->isRunning ? "Running" : "Stopped");
        curr = curr->next;
    }
    printf("Total Number of Processes: %d\n", numProcesses);

}

void updateBackgroundProcess() {
    int status;
    int pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED);
    if (pid > 0) {
        printf("pid %d changed with status %d\n", pid, status);
        if (WIFCONTINUED(status)) {
            //Continued
            //status 65535
            node* myNode = FindNode(pid);
            if (myNode) {
                myNode->isRunning = 1;
                printf("%d : Started\n", pid);
            }
        } else if (WIFSTOPPED(status)) {
            //Stopped
            //status 4991
            node* myNode = FindNode(pid);
            if (myNode) {
                myNode->isRunning = 0;
                printf("%d : Stopped\n", pid);
            }
        } else if (WIFEXITED(status)) {
            //Finished
            //status 256
            RemoveNode(pid);
            printf("%d : Finished\n", pid);
            //Remove it from the linkedList
        } else if (WIFSIGNALED(status)) {
            //Killed (RIP Harambe)
            //status 9
            RemoveNode(pid);
            printf("%d : Killed\n", pid);
            //Remove it from the linkedList
        }
    }
}

void splitOn(char* token, char contents[], char* result[]) {
    char* iterToken = strtok(contents, token);
    int i = 0;
    while (iterToken != NULL) {
        result[i] = strdup(iterToken);
        iterToken = strtok(NULL, token);
        i++;
    }
}

void PrintStat(int pid) {
    FILE* statfp;
    FILE* statusfp;
    char line[bufferSize];
    char* stat[bufferSize];
    char status[50][bufferSize];
    char* token = " ";
    char statBuffer[bufferSize];
    char statusBuffer[bufferSize];
    char dirBuffer[bufferSize];
    int nonvoluntary, voluntary;
    int i = 0;
    int j = 0;

    sprintf(dirBuffer, "/proc/%d", pid);
    DIR* dir = opendir(dirBuffer);

    if (!dir) {
        //Directory does not exist, and therefore the process.
        if (ENOENT == errno) {
            printf("Process %d does not exist.\n", pid);
        } else {
            printf("Process %d is unaccessible.\n", pid);
        }
        return;
    }

    //status
    sprintf(statusBuffer, "/proc/%d/status", pid);
    statusfp = fopen(statusBuffer, "r");

    while (fgets(status[j], 256, statusfp) != NULL) j++;

    sscanf(status[j - 2], "voluntary_ctxt_switches: %d", &voluntary);
    sscanf(status[j - 1], "nonvoluntary_ctxt_switches: %d", &nonvoluntary);

    //stat
    sprintf(statBuffer, "/proc/%d/stat", pid);
    statfp = fopen(statBuffer, "r");

    if (statfp == NULL) printf("Could not open proc for pid %d", pid);

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
    printf("voluntary_ctxt_switches: %d\n", voluntary);
    printf("nonvoluntary_ctxt_switches: %d\n", nonvoluntary);
}

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

void processInput(char* args[], int arglength) {
    int cp;
    char str[1000];
    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting\n");
        exit(1);
    } else  if (strcmp(args[0], "bg") == 0) {
        if (!args[1]) {
            printf("No job specified\n");
        } else {
            int id = fork();
            strcpy(str, "");
            for (cp = 1; cp < arglength; cp++) {
                strcat(str, args[cp]);
                strcat(str, " ");
            }
            if ( id == 0 ) {
                //Child Process
                execvp(args[1], &args[1]);
                printf("Failed to perform action %s\n", str);
                exit(1);
            } else if ( id > 0) {
                //Parent Process
                printf("Adding node with id %d and cmd %s\n", id, str);
                AddNode(id, str, 1);
                sleep(1);
            } else {
                //forking failed, join the dark side.
                printf("Forking Failed");
            }
        }
    } else if (strcmp(args[0], "bglist") == 0) {
        PrintLinkedList();
    } else if (strcmp(args[0], "bgkill") == 0) {
        if (verifyInput(args[1])) {
            KillProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "bgstop") == 0) {
        if (verifyInput(args[1])) {
            StopProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "bgstart") == 0) {
        if (verifyInput(args[1])) {
            StartProcess(atoi(args[1]));
        }
    } else if (strcmp(args[0], "pstat") == 0) {
        if (verifyInput(args[1])) {
            PrintStat(atoi(args[1]));
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
    while (1) {
        i = 0;
        char *input = NULL ;
        char *iterToken;
        char *args[maxArgs];


        updateBackgroundProcess();
        input = readline(prompt);

        if (!strcmp(input,"")) continue;

        iterToken = strtok(input, token);
        for (j = 0; j < maxArgs; j++) {
            if (iterToken) i++;
            args[j] = iterToken;
            iterToken = strtok(NULL, token);
        }

        processInput(args, i);
        updateBackgroundProcess();
    }
}
