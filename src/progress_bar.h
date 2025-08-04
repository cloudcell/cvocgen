#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <stdio.h>
#include <time.h>
#include <string.h>

typedef struct {
    int total;              // Total number of iterations
    int current;            // Current iteration
    int bar_width;          // Width of the progress bar
    time_t start_time;      // Start time
    time_t last_update;     // Last update time
    char prefix[50];        // Prefix string
    int last_printed_len;   // Length of the last printed line
} ProgressBar;

// Initialize a new progress bar
static inline ProgressBar progress_bar_init(const char* prefix, int total, int bar_width) {
    ProgressBar bar;
    bar.total = total;
    bar.current = 0;
    bar.bar_width = bar_width;
    bar.start_time = time(NULL);
    bar.last_update = bar.start_time;
    bar.last_printed_len = 0;
    
    strncpy(bar.prefix, prefix, sizeof(bar.prefix) - 1);
    bar.prefix[sizeof(bar.prefix) - 1] = '\0';
    
    return bar;
}

// Update the progress bar
static inline void progress_bar_update(ProgressBar* bar, int current) {
    bar->current = current;
    time_t now = time(NULL);
    
    // Only update the display if at least 1 second has passed since the last update
    // or if this is the first or last update
    if (now > bar->last_update || current == 0 || current >= bar->total) {
        bar->last_update = now;
        
        // Calculate progress
        float progress = (float)current / bar->total;
        int pos = bar->bar_width * progress;
        
        // Calculate time metrics
        time_t elapsed = now - bar->start_time;
        double iterations_per_sec = (elapsed > 0) ? (double)current / elapsed : 0;
        double sec_per_iteration = (current > 0) ? (double)elapsed / current : 0;
        
        // Calculate estimated time remaining
        time_t eta = (current > 0) ? (time_t)((bar->total - current) * sec_per_iteration) : 0;
        
        // Clear the previous line
        char clear_line[128];
        memset(clear_line, ' ', bar->last_printed_len);
        clear_line[bar->last_printed_len] = '\0';
        printf("\r%s", clear_line);
        
        // Print the progress bar
        printf("\r%s [", bar->prefix);
        for (int i = 0; i < bar->bar_width; i++) {
            if (i < pos) printf("=");
            else if (i == pos) printf(">");
            else printf(" ");
        }
        
        // Print the metrics
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer), 
                 "] %3d%% | %02ld:%02ld:%02ld | %.2f it/s | %.2f s/it | ETA: %02ld:%02ld:%02ld",
                 (int)(progress * 100),
                 elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60,
                 iterations_per_sec,
                 sec_per_iteration,
                 eta / 3600, (eta % 3600) / 60, eta % 60);
        
        printf("%s", buffer);
        fflush(stdout);
        
        // Store the length of the printed line
        bar->last_printed_len = strlen(bar->prefix) + 2 + bar->bar_width + len;
        
        // Print a newline when done
        if (current >= bar->total) {
            printf("\n");
        }
    }
}

// Increment the progress bar by one step
static inline void progress_bar_increment(ProgressBar* bar) {
    progress_bar_update(bar, bar->current + 1);
}

// Finish the progress bar
static inline void progress_bar_finish(ProgressBar* bar) {
    progress_bar_update(bar, bar->total);
}

#endif /* PROGRESS_BAR_H */
