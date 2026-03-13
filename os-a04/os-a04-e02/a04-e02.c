#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int page_number;
  int process_id;
  char access_type;
} MemoryAccess;

typedef struct {
  int is_used;
  int page_number;
  int process_id;
  int is_dirty;
  int last_access_step;
} Frame;

static int find_frame_index(Frame frames[], int number_of_frames,
                            int page_number, int process_id) {
  int frame_index;
  for (frame_index = 0; frame_index < number_of_frames; frame_index++) {
    if (frames[frame_index].is_used &&
        frames[frame_index].page_number == page_number &&
        frames[frame_index].process_id == process_id) {
      return frame_index;
    }
  }
  return -1;
}

static int find_empty_frame_index(Frame frames[], int number_of_frames) {
  int frame_index;
  for (frame_index = 0; frame_index < number_of_frames; frame_index++) {
    if (!frames[frame_index].is_used) {
      return frame_index;
    }
  }
  return -1;
}

static int find_next_use_step(MemoryAccess accesses[], int number_of_accesses,
                              int current_step, int page_number,
                              int process_id) {
  int look_ahead_step;
  for (look_ahead_step = current_step + 1; look_ahead_step < number_of_accesses;
       look_ahead_step++) {
    if (accesses[look_ahead_step].page_number == page_number &&
        accesses[look_ahead_step].process_id == process_id) {
      return look_ahead_step;
    }
  }
  return number_of_accesses + 1;
}

static int choose_optimal_victim_frame(Frame frames[], int number_of_frames,
                                       MemoryAccess accesses[],
                                       int number_of_accesses,
                                       int current_step) {
  int victim_frame_index = 0;
  int farthest_next_use_step = -1;
  int frame_index;

  for (frame_index = 0; frame_index < number_of_frames; frame_index++) {
    int next_use_step = find_next_use_step(
        accesses, number_of_accesses, current_step,
        frames[frame_index].page_number, frames[frame_index].process_id);

    if (next_use_step > farthest_next_use_step) {
      farthest_next_use_step = next_use_step;
      victim_frame_index = frame_index;
    }
  }

  return victim_frame_index;
}

static int compute_optimal_faults(MemoryAccess accesses[], int number_of_frames,
                                  int number_of_accesses) {
  Frame *optimal_frames;
  int total_optimal_faults = 0;
  int step_index;

  optimal_frames = (Frame *)calloc((size_t)number_of_frames, sizeof(Frame));
  if (optimal_frames == NULL) {
    return 0;
  }

  for (step_index = 0; step_index < number_of_accesses; step_index++) {
    int page_number = accesses[step_index].page_number;
    int process_id = accesses[step_index].process_id;
    int frame_index = find_frame_index(optimal_frames, number_of_frames,
                                       page_number, process_id);

    if (frame_index == -1) {
      int destination_frame_index =
          find_empty_frame_index(optimal_frames, number_of_frames);

      total_optimal_faults++;

      if (destination_frame_index == -1) {
        destination_frame_index = choose_optimal_victim_frame(
            optimal_frames, number_of_frames, accesses, number_of_accesses,
            step_index);
      }

      optimal_frames[destination_frame_index].is_used = 1;
      optimal_frames[destination_frame_index].page_number = page_number;
      optimal_frames[destination_frame_index].process_id = process_id;
    }
  }

  free(optimal_frames);
  return total_optimal_faults;
}

static int choose_lru_victim_frame(Frame frames[], int number_of_frames) {
  int victim_frame_index = 0;
  int frame_index;

  for (frame_index = 1; frame_index < number_of_frames; frame_index++) {
    if (frames[frame_index].last_access_step <
        frames[victim_frame_index].last_access_step) {
      victim_frame_index = frame_index;
    }
  }

  return victim_frame_index;
}

static void print_frames(Frame frames[], int number_of_frames) {
  int frame_index;
  for (frame_index = 0; frame_index < number_of_frames; frame_index++) {
    if (!frames[frame_index].is_used) {
      printf("-");
    } else {
      printf("P%d:%d", frames[frame_index].process_id,
             frames[frame_index].page_number);
    }

    if (frame_index < number_of_frames - 1) {
      printf(", ");
    }
  }
  printf("\n");
}

int main(void) {
  int number_of_frames;
  int number_of_processes;
  int number_of_accesses;
  MemoryAccess *all_accesses;
  Frame *frames;
  int *page_faults_per_process;
  int total_page_faults = 0;
  int writes_to_swap = 0;
  int optimal_faults;
  int step_index;
  int process_index;

  if (scanf("%d %d", &number_of_frames, &number_of_processes) != 2) {
    return 0;
  }

  if (scanf("%d", &number_of_accesses) != 1) {
    return 0;
  }

  all_accesses =
      (MemoryAccess *)malloc((size_t)number_of_accesses * sizeof(MemoryAccess));
  frames = (Frame *)calloc((size_t)number_of_frames, sizeof(Frame));
  page_faults_per_process =
      (int *)calloc((size_t)number_of_processes, sizeof(int));

  if (all_accesses == NULL || frames == NULL ||
      page_faults_per_process == NULL) {
    free(all_accesses);
    free(frames);
    free(page_faults_per_process);
    return 1;
  }

  for (step_index = 0; step_index < number_of_accesses; step_index++) {
    if (scanf("%d %d %c", &all_accesses[step_index].page_number,
              &all_accesses[step_index].process_id,
              &all_accesses[step_index].access_type) != 3) {
      free(all_accesses);
      free(frames);
      free(page_faults_per_process);
      return 0;
    }
  }

  optimal_faults = compute_optimal_faults(all_accesses, number_of_frames,
                                          number_of_accesses);

  printf("=== LRU ===\n");
  printf("Frames: %d, Processes: %d\n", number_of_frames, number_of_processes);

  for (step_index = 0; step_index < number_of_accesses; step_index++) {
    int page_number = all_accesses[step_index].page_number;
    int process_id = all_accesses[step_index].process_id;
    char access_type = all_accesses[step_index].access_type;
    int frame_index =
        find_frame_index(frames, number_of_frames, page_number, process_id);
    const char *status_text;

    if (frame_index != -1) {
      status_text = "HIT";
      frames[frame_index].last_access_step = step_index;
      if (access_type == 'W') {
        frames[frame_index].is_dirty = 1;
      }
    } else {
      int destination_frame_index =
          find_empty_frame_index(frames, number_of_frames);

      status_text = "FAULT";
      total_page_faults++;
      page_faults_per_process[process_id - 1]++;

      if (destination_frame_index == -1) {
        destination_frame_index =
            choose_lru_victim_frame(frames, number_of_frames);
        if (frames[destination_frame_index].is_dirty) {
          writes_to_swap++;
        }
      }

      frames[destination_frame_index].is_used = 1;
      frames[destination_frame_index].page_number = page_number;
      frames[destination_frame_index].process_id = process_id;
      frames[destination_frame_index].is_dirty = (access_type == 'W');
      frames[destination_frame_index].last_access_step = step_index;
    }

    printf("P%d %d %c: %-7s", process_id, page_number, access_type,
           status_text);
    print_frames(frames, number_of_frames);
  }

  printf("Page faults: %d (", total_page_faults);
  for (process_index = 0; process_index < number_of_processes;
       process_index++) {
    printf("P%d: %d", process_index + 1,
           page_faults_per_process[process_index]);
    if (process_index < number_of_processes - 1) {
      printf(", ");
    }
  }
  printf(")\n");
  printf("Writes to swap: %d\n", writes_to_swap);
  printf("Optimal faults: %d\n", optimal_faults);

  free(all_accesses);
  free(frames);
  free(page_faults_per_process);
  return 0;
}
