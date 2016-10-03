#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

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
        printf("pid %d change with status %d\n", pid, status);
        if (WIFCONTINUED(status)) {
            //Continued
            //status 65535
            node* myNode = FindNode(pid);
            if (myNode) {
                myNode->isRunning = 1;
            }
        } else if (WIFSTOPPED(status)) {
            //Stopped
            //status 4991
            node* myNode = FindNode(pid);
            if (myNode) {
                myNode->isRunning = 0;
            }
        } else if (WIFEXITED(status)) {
            //Finished
            //status 256
            RemoveNode(pid);
            //Remove it from the linkedList
        } else if (WIFSIGNALED(status)) {
            //Killed (RIP Harambe)
            //status 9
            RemoveNode(pid);
            //Remove it from the linkedList
        }
    }
}

void PrintStat(int pid) {
    FILE * statfp;
    FILE * statusfp;
    char line[10000];
    size_t len = 0;
    ssize_t read;
    char *stat[1024];
    char status[50][256];
    int i = 0;
    int j = 0;
    char* token = " ";
    char statBuffer[1000];
    char statusBuffer[1000];
    int nonvoluntary, voluntary;

    if (!FindNode(pid)) return;

    //status
    sprintf(statusBuffer, "/proc/%d/status", pid);
    statusfp = fopen(statusBuffer, "r");

    while (fgets(status[j], 256, statusfp) != NULL) j++;

    sscanf(status[j - 2], "voluntary_ctxt_switches: %d", &voluntary);
    sscanf(status[j - 1], "nonvoluntary_ctxt_switches: %d", &nonvoluntary);

    //stat
    sprintf(statBuffer, "/proc/%d/stat", pid);
    printf("%s place\n", statBuffer);
    statfp = fopen(statBuffer, "r");
    if (statfp == NULL)
        printf("Could not open proc for pid %d", pid);

    fgets(line, sizeof(line), statfp) != NULL;

    char* iterToken = strtok(line, token);
    while (iterToken != NULL) {
        stat[i] = strdup(iterToken);
        iterToken = strtok(NULL, token);
        i++;
    }

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

int main(){
    int id;
    int i;
    int j;
    int cp;
    char *token = " ";
    char *prompt = "your command: ";
    while (1) {
        updateBackgroundProcess();
        char *input = NULL ;

        char *iterToken;
        char *args[256];
        char str[1000];
        i = 0;
        cp = 0;
        input = readline(prompt);
        if (!strcmp(input,"")) continue;

        iterToken = strtok(input, token);
        while (iterToken != NULL) {
            args[i] = iterToken;
            iterToken = strtok(NULL, token);
            i++;
        }
        for (j = i; j < 256; j++) {
            //Clear out old args
            args[j] = NULL;
        }

        if (strcmp(args[0], "exit") == 0) {
            printf("Exiting\n");
            break;
        } else  if (strcmp(args[0], "bg") == 0) {
            if (!args[1]) {
                printf("No job specified\n");
            } else {
                int id = fork();
                strcpy(str, "");
                for (cp = 1; cp < i; cp++) {
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
        updateBackgroundProcess();
    }
    return 1;
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

