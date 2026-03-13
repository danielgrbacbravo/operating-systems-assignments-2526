
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_STOCKS 16
#define MAX_TXNS 32
#define NAME_LEN 9
#define LOG_LEN 256

typedef struct {
  char name[NAME_LEN];
  int price;
  int inventory;
} Stock;

typedef enum { CMD_BUY, CMD_SELL, CMD_EXCHANGE, CMD_UPDATE } CommandType;

typedef struct {
  CommandType type;

  char stock1[NAME_LEN];

  int price;

  int quantity;

  char stock2[NAME_LEN];
  int quantity2;
} Command;

typedef struct {
  int n_stocks;
  int n_cmds;

  Stock stocks[MAX_STOCKS];
  sem_t locks[MAX_STOCKS];     // one semaphore per stock
  char log[MAX_TXNS][LOG_LEN]; // one slot per transaction
} SharedData;

static void die(const char *msg) {
  perror(msg);
  exit(1);
}

static CommandType parse_type(const char *s) {
  if (strcmp(s, "BUY") == 0)
    return CMD_BUY;
  if (strcmp(s, "SELL") == 0)
    return CMD_SELL;
  if (strcmp(s, "EXCHANGE") == 0)
    return CMD_EXCHANGE;
  if (strcmp(s, "UPDATE_VALUATION") == 0)
    return CMD_UPDATE;

  fprintf(stderr, "Unknown command: %s\n", s);
  exit(1);
}

static int find_stock_index(const SharedData *shd, const char *name) {
  for (int i = 0; i < shd->n_stocks; i++) {
    if (strcmp(shd->stocks[i].name, name) == 0)
      return i;
  }
  return -1; // input guarantees existence, so this shouldn't happen
}

static void lock_one(SharedData *shd, int idx) {
  if (sem_wait(&shd->locks[idx]) != 0)
    die("sem_wait");
}

static void unlock_one(SharedData *shd, int idx) {
  if (sem_post(&shd->locks[idx]) != 0)
    die("sem_post");
}

static void lock_two_ordered(SharedData *shd, int a, int b) {
  // deadlock prevention: lock lower index first (required)
  if (a < b) {
    lock_one(shd, a);
    lock_one(shd, b);
  } else if (b < a) {
    lock_one(shd, b);
    lock_one(shd, a);
  } else {
    // same stock (not expected for EXCHANGE), lock once
    lock_one(shd, a);
  }
}

static void unlock_two_ordered(SharedData *shd, int a, int b) {

  if (a < b) {
    unlock_one(shd, b);
    unlock_one(shd, a);
  } else if (b < a) {
    unlock_one(shd, a);
    unlock_one(shd, b);
  } else {
    unlock_one(shd, a);
  }
}

static void exec_buy(int tid, SharedData *shd, const Command *c) {
  int idx = find_stock_index(shd, c->stock1);

  lock_one(shd, idx);
  int P = shd->stocks[idx].price;
  if (P <= c->price) {
    shd->stocks[idx].inventory += c->quantity;
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] BUY %s : bought %d share (s) at %d (Success)", tid,
             c->stock1, c->quantity, P);
  } else {
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] BUY %s : price %d exceeds maximum %d (Failed)", tid,
             c->stock1, P, c->price);
  }
  unlock_one(shd, idx);
}

static void exec_sell(int tid, SharedData *shd, const Command *c) {
  int idx = find_stock_index(shd, c->stock1);

  lock_one(shd, idx);
  int P = shd->stocks[idx].price;
  int I = shd->stocks[idx].inventory;

  if (P < c->price) {
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] SELL %s : price %d below minimum %d (Failed)", tid,
             c->stock1, P, c->price);
  } else if (I < c->quantity) {
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] SELL %s : insufficient inventory %d (Failed)", tid,
             c->stock1, I);
  } else {
    shd->stocks[idx].inventory -= c->quantity;
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] SELL %s : sold %d share (s) at %d (Success)", tid, c->stock1,
             c->quantity, P);
  }
  unlock_one(shd, idx);
}

static void exec_update(int tid, SharedData *shd, const Command *c) {
  int idx = find_stock_index(shd, c->stock1);

  lock_one(shd, idx);
  shd->stocks[idx].price = c->price;
  snprintf(shd->log[tid], LOG_LEN,
           "[%d] UPDATE_VALUATION %s : price updated to %d (Success)", tid,
           c->stock1, c->price);
  unlock_one(shd, idx);
}

static void exec_exchange(int tid, SharedData *shd, const Command *c) {
  int src = find_stock_index(shd, c->stock1);
  int dst = find_stock_index(shd, c->stock2);

  lock_two_ordered(shd, src, dst);

  int srcInv = shd->stocks[src].inventory;
  int srcP = shd->stocks[src].price;
  int dstP = shd->stocks[dst].price;

  if (srcInv < c->quantity) {
    snprintf(shd->log[tid], LOG_LEN,
             "[%d] EXCHANGE %s %s : insufficient inventory %d (Failed)", tid,
             c->stock1, c->stock2, srcInv);
  } else {
    long lhs = (long)c->quantity2 * (long)dstP;
    long rhs = (long)c->quantity * (long)srcP;

    if (lhs < rhs) {
      snprintf(shd->log[tid], LOG_LEN,
               "[%d] EXCHANGE %s %s : disadvantageous trade (Failed)", tid,
               c->stock1, c->stock2);
    } else {
      shd->stocks[src].inventory -= c->quantity;
      shd->stocks[dst].inventory += c->quantity2;

      snprintf(shd->log[tid], LOG_LEN,
               "[%d] EXCHANGE %s %s : exchanged %d share (s) of %s for %d "
               "share (s) of %s (Success)",
               tid, c->stock1, c->stock2, c->quantity, c->stock1, c->quantity2,
               c->stock2);
    }
  }

  unlock_two_ordered(shd, src, dst);
}

static void execute_transaction(int tid, SharedData *shd, const Command *c) {
  switch (c->type) {
  case CMD_BUY:
    exec_buy(tid, shd, c);
    break;
  case CMD_SELL:
    exec_sell(tid, shd, c);
    break;
  case CMD_UPDATE:
    exec_update(tid, shd, c);
    break;
  case CMD_EXCHANGE:
    exec_exchange(tid, shd, c);
    break;
  default:
    _exit(1);
  }
}

static void print_header(const SharedData *shd) {
  printf("=== BROKERAGE ENGINE ===\n");
  printf("Stocks : %d\n", shd->n_stocks);
  for (int i = 0; i < shd->n_stocks; i++) {
    printf("%s : price %d , inventory %d\n", shd->stocks[i].name,
           shd->stocks[i].price, shd->stocks[i].inventory);
  }
}

static void print_log_and_final(const SharedData *shd) {
  printf("=== TRANSACTION LOG ===\n");
  for (int i = 0; i < shd->n_cmds; i++) {
    printf("%s\n", shd->log[i]);
  }
  printf("=== FINAL PORTFOLIO ===\n");
  for (int i = 0; i < shd->n_stocks; i++) {
    printf("%s : price %d , inventory %d\n", shd->stocks[i].name,
           shd->stocks[i].price, shd->stocks[i].inventory);
  }
}

int main(void) {
  Stock localStocks[MAX_STOCKS];
  Command cmds[MAX_TXNS];

  int N;
  if (scanf("%d", &N) != 1)
    return 0;
  if (N < 1 || N > MAX_STOCKS) {
    fprintf(stderr, "Bad N\n");
    return 1;
  }

  for (int i = 0; i < N; i++) {
    if (scanf("%8s %d %d", localStocks[i].name, &localStocks[i].price,
              &localStocks[i].inventory) != 3) {
      fprintf(stderr, "Bad stock line\n");
      return 1;
    }
  }

  int M;
  if (scanf("%d", &M) != 1) {
    fprintf(stderr, "Bad M\n");
    return 1;
  }
  if (M < 1 || M > MAX_TXNS) {
    fprintf(stderr, "Bad M\n");
    return 1;
  }

  for (int i = 0; i < M; i++) {
    char type[32];
    if (scanf("%31s", type) != 1)
      die("scanf cmd");

    cmds[i].type = parse_type(type);

    if (cmds[i].type == CMD_BUY || cmds[i].type == CMD_SELL) {
      scanf("%8s %d %d", cmds[i].stock1, &cmds[i].price, &cmds[i].quantity);
      cmds[i].stock2[0] = '\0';
      cmds[i].quantity2 = 0;
    } else if (cmds[i].type == CMD_UPDATE) {
      scanf("%8s %d", cmds[i].stock1, &cmds[i].price);
      cmds[i].quantity = 0;
      cmds[i].stock2[0] = '\0';
      cmds[i].quantity2 = 0;
    } else if (cmds[i].type == CMD_EXCHANGE) {
      scanf("%8s %d %8s %d", cmds[i].stock1, &cmds[i].quantity, cmds[i].stock2,
            &cmds[i].quantity2);
      cmds[i].price = 0;
    }
  }

  // ---- shared memory setup ----
  const char *SHM_NAME = "/brokerage_shm_naive";
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd < 0)
    die("shm_open");

  if (ftruncate(fd, sizeof(SharedData)) != 0)
    die("ftruncate");

  SharedData *shd =
      mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shd == MAP_FAILED)
    die("mmap");
  close(fd);

  // init shared data
  memset(shd, 0, sizeof(*shd));
  shd->n_stocks = N;
  shd->n_cmds = M;
  for (int i = 0; i < N; i++)
    shd->stocks[i] = localStocks[i];
  for (int i = 0; i < M; i++)
    shd->log[i][0] = '\0';

  // init semaphores (must be BEFORE fork)
  for (int i = 0; i < N; i++) {
    if (sem_init(&shd->locks[i], 1, 1) != 0)
      die("sem_init");
  }

  print_header(shd);
  printf("\n");
  fflush(stdout);

  pid_t pids[MAX_TXNS];
  for (int i = 0; i < M; i++) {
    pid_t pid = fork();
    if (pid < 0)
      die("fork");

    if (pid == 0) {
      // child
      execute_transaction(i, shd, &cmds[i]);
      _exit(0);
    } else {
      // parent
      pids[i] = pid;
    }
  }

  // ---- wait for all children ----
  for (int i = 0; i < M; i++) {
    int status = 0;
    waitpid(pids[i], &status, 0);
  }

  // print log + final
  printf("=== TRANSACTION LOG ===\n");
  for (int i = 0; i < M; i++) {
    printf("%s\n", shd->log[i]);
  }
  printf("\n");
  printf("=== FINAL PORTFOLIO ===\n");
  for (int i = 0; i < N; i++) {
    printf("%s : price %d , inventory %d\n", shd->stocks[i].name,
           shd->stocks[i].price, shd->stocks[i].inventory);
  }

  // ---- cleanup ----
  for (int i = 0; i < N; i++) {
    sem_destroy(&shd->locks[i]);
  }
  munmap(shd, sizeof(SharedData));
  shm_unlink(SHM_NAME);

  return 0;
}
