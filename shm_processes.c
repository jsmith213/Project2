#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
 
#define MAX_KIDS 64
 
typedef struct{
  int BankAccount;
  sem_t mutex;
} Shared;

void DearOldDad(Shared *ShmPTR);
void PoorStudent(Shared *ShmPTR, int id, int multiple);
void LovableMom(Shared *ShmPTR);

static pid_t kids[MAX_KIDS + 1];
static int nkids = 0;
static volatile sig_atomic_t stop = 0;
static void parent_handler(int sig) { (void)sig; stop = 1;}
static void P(sem_t *s) { while (sem_wait(s) == -1 && errno == EINTR);}
static void V(sem_t *s) { sem_post(s);}
 
 
int main(int argc, char *argv[])
{
     int   ShmID;
     Shared *ShmPTR;
     pid_t pid;
     
     int parents = 1, students = 1;
     int i;
     if (argc == 3){
      parents = atoi(argv[1]);
      students = atoi(argv[2]);
     } else if (argc !=1){
      fprintf(stderr, "Use: %s [1|2] [#students]\n", argv[0]);
      exit(1);
     }
     if (parents < 1 || parents > 2 || students < 1 || students > MAX_KIDS){
      fprintf(stderr, "Use: %s [1|2] [1-%d students]\n", argv[0], MAX_KIDS);
      exit(1);
     }
 
     ShmID = shmget(IPC_PRIVATE, sizeof(Shared), IPC_CREAT | 0666);
     if (ShmID < 0) {
          printf("*** shmget error (server) ***\n");
          exit(1);
     }
 
     ShmPTR = (Shared *) shmat(ShmID, NULL, 0);
     if (ShmPTR == (Shared *) -1) {
          printf("*** shmat error (server) ***\n");
          exit(1);
     }
 
     ShmPTR->BankAccount;   /* BankAccount = 0 */
     
     if (sem_init(&ShmPTR->mutex, 1, 1) == -1){
      perror("sem_init");
      exit(1);
     }
 
     setvbuf(stdout, NULL, _IOLBF, 0);

     if (parents == 2){
      pid = fork();
      if (pid < 0) { printf("*** fork error (server) ***\n"); exit(1);
        } else if (pid == 0){
          srand(time(NULL) ^ (getpid() << 8));
          LovableMom(ShmPTR);
          exit(0);
        } 
      kids[nkids++] = pid;

     }

     for (i = 1; i <= students; i++){
        pid = fork();
        if (pid < 0) {
              printf("*** fork error (server) ***\n");
              exit(1);
        }
        else if (pid == 0) {
              srand(time(NULL) ^ (getpid() << 8));
              PoorStudent(ShmPTR);
              exit(0);
        }
        kids[nkids++] = pid;
     }
 
     

     struct sigaction sa;
     memset(&sa, 0, sizeof(sa));
     sa.sa_handler = parent_handler;
     sigemptyset(&sa.sa_mask);
     sigaction(SIGINT,  &sa, NULL);
     sigaction(SIGTERM, &sa, NULL);
 
     /* Parent is Dear Old Dad */
     srand(time(NULL) ^ (getpid() << 8));
     DearOldDad(ShmPTR);
 
     for (i = 0; i < nkids; i++) kill(kids[i], SIGKILL);
     for (i = 0; i < nkids; i++) waitpid(kids[i], NULL, 0);
     sem_destroy(&ShmPTR->mutex);
     shmdt((void *) ShmPTR);
     shmctl(ShmID, IPC_RMID, NULL);
     exit(0);
}
 
void DearOldDad(Shared *ShmPTR)
{
     int localBalance = 0;
     int amount;
 
     while (!stop) {
          sleep(rand() % 6);
          if (stop) break;
          printf("Dear Old Dad: Attempting to Check Balance\n");
 
          if (rand() % 2 == 0) {
               if (localBalance < 100) {
                    P(&ShmPTR->mutex);
                    localBalance = ShmPTR->BankAccount;
                    amount = rand() % 101;
                    if (rand() % 2 == 0) {
                         localBalance += amount;
                         printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                                amount, localBalance);
                         ShmPTR->BankAccount = localBalance;
                    } else {
                         printf("Dear old Dad: Doesn't have any money to give\n");
                    }
                    V(&ShmPTR->mutex);
               } else {
                    printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                           localBalance);
               }
          } else {
               P(&ShmPTR->mutex);
               localBalance = ShmPTR->BankAccount;
               V(&ShmPTR->mutex);
               printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
          }
     }
}

void LovableMom(Shared *ShmPTR){
  int localBalance = 0;
  int amount;

  for (;;) {
    sleep(rand() % 11);
    printf("Loveable Mom: Attempting to Check Balance\n");
    P(&ShmPTR->mutex);
    localBalance = ShmPTR->BankAccount;
    if (localBalance <= 100){
      amount = rand() % 126;
      localBalance += amount;
      printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
      ShmPTR->BankAccount = localBalance;
    }
    V(&ShmPTR->mutex);
  }
}

 
void PoorStudent(Shared *ShmPTR, int id, int multiple)
{
     int localBalance = 0;
     int need;
     char name[32];
 
     if (multiple) snprintf(name, sizeof(name), "Poor Student #%d", id);
     else          snprintf(name, sizeof(name), "Poor Student");
 
     for (;;) {
          sleep(rand() % 6);
          printf("%s: Attempting to Check Balance\n", name);
 
          if (rand() % 2 == 0) {
               P(&ShmPTR->mutex);
               localBalance = ShmPTR->BankAccount;
               need = rand() % 51;
               printf("%s needs $%d\n", name, need);
               if (need <= localBalance) {
                    localBalance -= need;
                    printf("%s: Withdraws $%d / Balance = $%d\n",
                           name, need, localBalance);
                    ShmPTR->BankAccount = localBalance;
               } else {
                    printf("%s: Not Enough Cash ($%d)\n", name, localBalance);
               }
               V(&ShmPTR->mutex);
          } else {
               P(&ShmPTR->mutex);
               localBalance = ShmPTR->BankAccount;
               V(&ShmPTR->mutex);
               printf("%s: Last Checking Balance = $%d\n", name, localBalance);
          }
     }
}
 
