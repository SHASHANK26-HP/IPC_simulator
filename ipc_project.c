#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

// ANSI Colour Codes for Terminal UI
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CYAN    "\033[1;36m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define RED     "\033[1;31m"

typedef struct {
    char message[256];
    int secret_code;
    sem_t sem;
} SharedBlock;

static void print_header(const char *title) {
    printf("\n" CYAN "+----------------------------------------------------------+\n");
    printf("| %-56s |\n", title);
    printf("+----------------------------------------------------------+" RESET "\n");
}

void run_pipe_interface() {
    int fd[2];
    char input_msg[256];

    print_header("MODE 1 : MESSAGE PASSING (ANONYMOUS PIPE)");
    printf(BOLD "Enter a message for Parent to send to Child: " RESET);
    getchar(); // Clear leftover newline from menu input
    fgets(input_msg, sizeof(input_msg), stdin);
    input_msg[strcspn(input_msg, "\n")] = 0;

    if (pipe(fd) == -1) {
        perror(RED "Pipe creation failed" RESET);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror(RED "Fork failed" RESET);
        return;
    }

    if (pid > 0) {
        close(fd[0]);
        printf("\n" BLUE "[PARENT | PID %d]" RESET " Writing to pipe write-end (fd[1])...\n", getpid());
        write(fd[1], input_msg, strlen(input_msg) + 1);
        close(fd[1]);
        wait(NULL);
        printf(BLUE "[PARENT | PID %d]" RESET " Child completed. Pipe closed cleanly.\n", getpid());
    } else {
        close(fd[1]);
        char received_msg[256];
        read(fd[0], received_msg, sizeof(received_msg));
        printf(GREEN "[CHILD  | PID %d]" RESET " Read from pipe read-end (fd[0]): " BOLD "\"%s\"" RESET "\n",
               getpid(), received_msg);
        close(fd[0]);
        exit(0);
    }
}

void run_shared_memory_interface() {
    char input_msg[256];
    int input_num;

    print_header("MODE 2 : SHARED MEMORY (MMAP) + SEMAPHORE");
    printf(BOLD "Enter text to store in Shared RAM: " RESET);
    getchar();
    fgets(input_msg, sizeof(input_msg), stdin);
    input_msg[strcspn(input_msg, "\n")] = 0;

    printf(BOLD "Enter an integer value to store in Shared RAM: " RESET);
    scanf("%d", &input_num);

    SharedBlock *shm = mmap(NULL, sizeof(SharedBlock),
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (shm == MAP_FAILED) {
        perror(RED "mmap failed" RESET);
        return;
    }

    sem_init(&shm->sem, 1, 0);

    pid_t pid = fork();
    if (pid < 0) {
        perror(RED "Fork failed" RESET);
        return;
    }

    if (pid > 0) {
        printf("\n" BLUE "[PARENT | PID %d]" RESET " Writing data to shared memory...\n", getpid());
        sleep(1);
        strcpy(shm->message, input_msg);
        shm->secret_code = input_num;

        printf(YELLOW "[SYNC   | PID %d]" RESET " Data ready -> calling sem_post() to unlock Child.\n", getpid());
        sem_post(&shm->sem);
        wait(NULL);

        sem_destroy(&shm->sem);
        munmap(shm, sizeof(SharedBlock));
        printf(BLUE "[PARENT | PID %d]" RESET " Shared memory unmapped and cleaned up.\n", getpid());
    } else {
        printf(YELLOW "[SYNC   | PID %d]" RESET " Waiting at sem_wait() until Parent finishes writing...\n", getpid());
        sem_wait(&shm->sem);

        printf(GREEN "[CHILD  | PID %d]" RESET " Unlocked! Read from RAM -> Text: " BOLD "\"%s\"" RESET
               " | Number: " BOLD "%d" RESET "\n",
               getpid(), shm->message, shm->secret_code);
        exit(0);
    }
}

int main() {
    int choice;

    while (1) {
        printf("\n" CYAN "============================================================\n");
        printf("        LINUX INTER-PROCESS COMMUNICATION (IPC) DEMO        \n");
        printf("============================================================" RESET "\n");
        printf("  " BOLD "1." RESET " Message Passing Demo   (Anonymous Pipe)\n");
        printf("  " BOLD "2." RESET " Shared Memory Demo     (mmap + POSIX Semaphore)\n");
        printf("  " BOLD "3." RESET " Exit\n");
        printf(CYAN "------------------------------------------------------------" RESET "\n");
        printf(BOLD "Select an option [1-3]: " RESET);

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf(RED "Invalid input. Please enter 1, 2, or 3.\n" RESET);
            continue;
        }

        switch (choice) {
            case 1:
                run_pipe_interface();
                break;
            case 2:
                run_shared_memory_interface();
                break;
            case 3:
                printf("\n" GREEN "Exiting IPC Demo. Good luck with your presentation!\n" RESET);
                return 0;
            default:
                printf(RED "\nInvalid choice. Please select 1, 2, or 3.\n" RESET);
        }
    }
}