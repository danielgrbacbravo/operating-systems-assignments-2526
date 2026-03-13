/*
 * Name: Daniel Grbac Bravo
 * Student ID: S5482585
 *
 * You must include this in your submission.
 */

/* Uncomment to enable the ANSI terminal visualiser (not used by the judge) */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_SIZE 1024
#define CHUNK 16

typedef struct {
  int rows, cols;
  unsigned char cells[MAX_SIZE][MAX_SIZE];
} GridType;

typedef struct {
  int top, left, bottom, right;
} Box;

static inline int box_is_empty(Box b) { return (b.top < 0); }

typedef struct {
  Box *chunks;
  int count;
} ChunkList;

static long histogram[9];
static pthread_mutex_t histogram_mutex = PTHREAD_MUTEX_INITIALIZER;

ChunkList compute_chunks(Box box) {
  ChunkList chunk_list = {.chunks = NULL, .count = 0};
  if (box_is_empty(box))
    return chunk_list;

  const int box_top = box.top, box_bottom = box.bottom;
  const int box_left = box.left, box_right = box.right;

  const int box_height = box_bottom - box_top + 1;
  const int box_width = box_right - box_left + 1;

  const int num_chunk_rows = (box_height + CHUNK - 1) / CHUNK;
  const int num_chunk_cols = (box_width + CHUNK - 1) / CHUNK;

  chunk_list.count = num_chunk_rows * num_chunk_cols;
  chunk_list.chunks = malloc((size_t)chunk_list.count * sizeof(Box));
  if (!chunk_list.chunks) {
    chunk_list.count = 0;
    return chunk_list;
  }

  int chunk_index = 0;
  for (int chunk_row = 0; chunk_row < num_chunk_rows; chunk_row++) {
    int chunk_start_row = box_top + chunk_row * CHUNK;
    int chunk_end_row = chunk_start_row + CHUNK;
    int max_row_exclusive = box_bottom + 1;
    if (chunk_end_row > max_row_exclusive)
      chunk_end_row = max_row_exclusive;

    for (int chunk_col = 0; chunk_col < num_chunk_cols; chunk_col++) {
      int chunk_start_col = box_left + chunk_col * CHUNK;
      int chunk_end_col = chunk_start_col + CHUNK;
      int max_col_exclusive = box_right + 1;
      if (chunk_end_col > max_col_exclusive)
        chunk_end_col = max_col_exclusive;

      chunk_list.chunks[chunk_index++] = (Box){.top = chunk_start_row,
                                               .left = chunk_start_col,
                                               .bottom = chunk_end_row - 1,
                                               .right = chunk_end_col - 1};
    }
  }
  return chunk_list;
}
static inline Box empty_box(void) {
  Box b;
  b.top = -1;
  b.left = 0;
  b.bottom = -1;
  b.right = -1;
  return b;
}

static Box compute_bounding_box(const GridType *grid) {
  int min_row = grid->rows, min_col = grid->cols;
  int max_row = -1, max_col = -1;

  for (int row = 0; row < grid->rows; row++) {
    for (int col = 0; col < grid->cols; col++) {
      if (grid->cells[row][col]) {
        if (row < min_row) {
          min_row = row;
        }
        if (col < min_col) {
          min_col = col;
        }
        if (row > max_row) {
          max_row = row;
        }
        if (col > max_col) {
          max_col = col;
        }
      }
    }
  }

  if (max_row < 0) {
    return empty_box();
  }

  Box bounding_box;
  bounding_box.top = min_row;
  bounding_box.left = min_col;
  bounding_box.bottom = max_row;
  bounding_box.right = max_col;
  return bounding_box;
}

static Box expand_box_clamped(Box bounding_box, int total_rows,
                              int total_cols) {
  if (box_is_empty(bounding_box)) {
    return bounding_box;
  }

  if (bounding_box.top > 0) {
    bounding_box.top--;
  }
  if (bounding_box.left > 0) {
    bounding_box.left--;
  }
  if (bounding_box.bottom < total_rows - 1) {
    bounding_box.bottom++;
  }
  if (bounding_box.right < total_cols - 1) {
    bounding_box.right++;
  }
  return bounding_box;
}

static inline int count_neighbours(const GridType *grid, int cell_row,
                                   int cell_col) {
  int neighbour_count = 0;
  for (int row_offset = -1; row_offset <= 1; row_offset++) {
    for (int col_offset = -1; col_offset <= 1; col_offset++) {
      if (row_offset == 0 && col_offset == 0) {
        continue;
      }
      int neighbour_row = cell_row + row_offset;
      int neighbour_col = cell_col + col_offset;
      if (neighbour_row < 0 || neighbour_row >= grid->rows ||
          neighbour_col < 0 || neighbour_col >= grid->cols) {
        continue;
      }
      neighbour_count += (grid->cells[neighbour_row][neighbour_col] ? 1 : 0);
    }
  }
  return neighbour_count;
}
// everythin the thread needs to know
typedef struct {
  const GridType *current;
  GridType *next;
  Box chunk;
  long local_hist[9];
} thread_arg_t;

static void *worker(void *void_pointer) {
  thread_arg_t *thread_arguments = (thread_arg_t *)void_pointer;
  memset(thread_arguments->local_hist, 0, sizeof(thread_arguments->local_hist));
  // For each cell in the assigned chunk
  for (int row = thread_arguments->chunk.top;
       row <= thread_arguments->chunk.bottom; row++) {
    for (int col = thread_arguments->chunk.left;
         col <= thread_arguments->chunk.right; col++) {
      // Count how many neighbors there are
      int neighbor_count =
          count_neighbours(thread_arguments->current, row, col);
      // Increment the thread's local histogram
      thread_arguments->local_hist[neighbor_count]++;

      // Determine if the cell will be alive in the next generation
      unsigned char is_currently_alive =
          thread_arguments->current->cells[row][col];
      unsigned char will_be_alive_next = 0;

      if (!is_currently_alive) {
        if (neighbor_count == 3) {
          will_be_alive_next = 1;
        }
      } else {
        if (neighbor_count == 2 || neighbor_count == 3) {
          will_be_alive_next = 1;
        }
      }

      thread_arguments->next->cells[row][col] = will_be_alive_next;
    }
  }
  pthread_mutex_lock(&histogram_mutex);
  for (int i = 0; i < 9; i++) {
    histogram[i] += thread_arguments->local_hist[i];
  }
  pthread_mutex_unlock(&histogram_mutex);

  return NULL;
}
void simulate_generation(const GridType *current, GridType *next) {
  next->rows = current->rows;
  next->cols = current->cols;

  // Ensure cells outside expanded bounding box are dead
  memset(next->cells, 0, sizeof(next->cells));

  memset(histogram, 0, sizeof(histogram));

  Box bounding_box = compute_bounding_box(current);
  if (box_is_empty(bounding_box)) {
    histogram[0] = (long)current->rows * (long)current->cols;
    return;
  }

  Box expanded_bounding_box =
      expand_box_clamped(bounding_box, current->rows, current->cols);
  ChunkList chunk_list = compute_chunks(expanded_bounding_box);

  pthread_t *threads = malloc((size_t)chunk_list.count * sizeof(pthread_t));
  thread_arg_t *thread_arguments =
      malloc((size_t)chunk_list.count * sizeof(thread_arg_t));

  for (int chunk_index = 0; chunk_index < chunk_list.count; chunk_index++) {
    thread_arguments[chunk_index].current = current;
    thread_arguments[chunk_index].next = next;
    thread_arguments[chunk_index].chunk = chunk_list.chunks[chunk_index];

    pthread_create(&threads[chunk_index], NULL, worker,
                   &thread_arguments[chunk_index]);
  }

  for (int chunk_index = 0; chunk_index < chunk_list.count; chunk_index++) {
    pthread_join(threads[chunk_index], NULL);
  }

  long number_of_cells_in_box =
      (long)(expanded_bounding_box.bottom - expanded_bounding_box.top + 1) *
      (long)(expanded_bounding_box.right - expanded_bounding_box.left + 1);
  long total_number_of_cells = (long)current->rows * (long)current->cols;
  long number_of_cells_outside_box =
      total_number_of_cells - number_of_cells_in_box;
  if (number_of_cells_outside_box > 0)
    histogram[0] += number_of_cells_outside_box;

  free(threads);
  free(thread_arguments);
  free(chunk_list.chunks);
}

static void print_grid(const GridType *g) {
  for (int r = 0; r < g->rows; r++) {
    for (int c = 0; c < g->cols; c++) {
      putchar(g->cells[r][c] ? '#' : '.');
    }
    putchar('\n');
  }
}

static void print_histogram(int gen) {
  printf("Generation %d:", gen);
  for (int i = 0; i < 9; i++) {
    printf(" %ld", histogram[i]);
  }
  printf("\n");
}

#ifdef VISUALIZE
static void display(const GridType *g, int gen) {
  printf("\033[2J\033[H");
  printf("Generation %d\n", gen);
  int dr = g->rows < 60 ? g->rows : 60;
  int dc = g->cols < 120 ? g->cols : 120;
  for (int r = 0; r < dr; r++) {
    for (int c = 0; c < dc; c++) {
      putchar(g->cells[r][c] ? '#' : '.');
    }
    putchar('\n');
  }
  print_histogram(gen);
  fflush(stdout);
  usleep(200000);
}
#endif

static void read_input(GridType *g, int *generations) {
  int n_alive;
  scanf("%d %d", &g->rows, &g->cols);
  scanf("%d", &n_alive);
  memset(g->cells, 0, sizeof(g->cells));
  for (int i = 0; i < n_alive; i++) {
    int r, c;
    scanf("%d %d", &r, &c);
    g->cells[r][c] = 1;
  }
  scanf("%d", generations);
}

int main(void) {
  static GridType grid_a, grid_b;
  int generations;

  read_input(&grid_a, &generations);
  grid_b.rows = grid_a.rows;
  grid_b.cols = grid_a.cols;

  GridType *current = &grid_a;
  GridType *next = &grid_b;

  for (int gen = 1; gen <= generations; gen++) {
    memset(next->cells, 0, sizeof(next->cells));

    simulate_generation(current, next);

    GridType *tmp = current;
    current = next;
    next = tmp;

    print_histogram(gen);
#ifdef VISUALIZE
    display(current, gen);
#endif
  }

  print_grid(current);
  return 0;
}
