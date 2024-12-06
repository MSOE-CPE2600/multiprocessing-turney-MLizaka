/**
 * Andrew Lizak
 * FILE: mandlemovie.c
 * Date: 12/5/2024
 * LAB 12
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include <time.h>
#include "jpegrw.h"

// Constants for the animation
#define FRAMES 100
#define INITIAL_SCALE 4.0
#define FINAL_SCALE 0.0001
#define INITIAL_X -0.7463
#define INITIAL_Y 0.1102
#define FINAL_X -0.7463
#define FINAL_Y 0.1102

// Struct to hold data for each thread
typedef struct {
    imgRawImage *image;
    double xmin, xmax, ymin, ymax;
    int start_row, end_row, max_iterations;
} thread_data_t;

// Function to calculate the number of iterations for a point in the Mandelbrot set
int iterations_at_point(double x, double y, int max_iterations) {
    double zx = 0.0, zy = 0.0;
    int iterations = 0;

    // Iterate to check if the point escapes the set
    while ((zx * zx + zy * zy < 4.0) && (iterations < max_iterations)) {
        double temp = zx * zx - zy * zy + x;
        zy = 2.0 * zx * zy + y;
        zx = temp;
        iterations++;
    }

    return iterations;
}

// Function to compute a portion of the image in a separate thread
void *compute_region(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    imgRawImage *image = data->image;
    int width = image->width;
    int height = image->height;

    // Iterate over the rows assigned to this thread
    for (int j = data->start_row; j < data->end_row; j++) {
        for (int i = 0; i < width; i++) {
            double x = data->xmin + i * (data->xmax - data->xmin) / width;
            double y = data->ymin + j * (data->ymax - data->ymin) / height;
            int iters = iterations_at_point(x, y, data->max_iterations);
            unsigned int color = (iters * 255 / data->max_iterations);
            setPixelCOLOR(image, i, j, (color << 16) | (color << 8) | color); // Grayscale color
        }
    }
    return NULL;
}

// Function to compute the entire image, split into multiple threads
void compute_image(imgRawImage *image, double xmin, double xmax, double ymin, double ymax, int max_iterations, int num_threads) {
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];

    int height = image->height;
    int rows_per_thread = height / num_threads;

    // Create threads to compute different regions of the image
    for (int t = 0; t < num_threads; t++) {
        thread_data[t].image = image;
        thread_data[t].xmin = xmin;
        thread_data[t].xmax = xmax;
        thread_data[t].ymin = ymin;
        thread_data[t].ymax = ymax;
        thread_data[t].max_iterations = max_iterations;
        thread_data[t].start_row = t * rows_per_thread;
        thread_data[t].end_row = (t == num_threads - 1) ? height : (t + 1) * rows_per_thread;

        if (pthread_create(&threads[t], NULL, compute_region, &thread_data[t]) != 0) {
            perror("Error creating thread");
            exit(1);
        }
    }

    // Wait for all threads to finish
    for (int t = 0; t < num_threads; t++) {
        if (pthread_join(threads[t], NULL) != 0) {
            perror("Error joining thread");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_processes> <num_threads>\n", argv[0]);
        exit(1);
    }

    int num_processes = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    // Check if the number of processes and threads are within reasonable limits
    if (num_processes < 1 || num_processes > FRAMES || num_threads < 1 || num_threads > 20) {
        fprintf(stderr, "Number of processes must be between 1 and %d, and number of threads must be between 1 and 20\n", FRAMES);
        exit(1);
    }

    // Calculate the scaling and movement steps for each frame
    double scale_step = pow(FINAL_SCALE / INITIAL_SCALE, 1.0 / FRAMES);
    double x_step = (FINAL_X - INITIAL_X) / FRAMES;
    double y_step = (FINAL_Y - INITIAL_Y) / FRAMES;

    double current_scale = INITIAL_SCALE;
    double current_x = INITIAL_X;
    double current_y = INITIAL_Y;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int active_processes = 0;

    // Loop to generate each frame
    for (int frame = 0; frame < FRAMES; frame++) {
        char outfile[50];
        snprintf(outfile, sizeof(outfile), "frame%03d.jpg", frame + 1);

        // Wait for a process to finish if we've hit the process limit
        if (active_processes >= num_processes) {
            wait(NULL);
            active_processes--;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Error during fork");
            exit(1);
        } else if (pid == 0) {
            // Child process: generate the image for this frame
            printf("Child process generating frame %d\n", frame);
            imgRawImage *image = initRawImage(1000, 1000);
            if (!image) {
                fprintf(stderr, "Failed to create image for frame %d\n", frame);
                exit(1);
            }

            compute_image(image, current_x - current_scale, current_x + current_scale,
                          current_y - current_scale, current_y + current_scale,
                          1000, num_threads);

            if (storeJpegImageFile(image, outfile) != 0) {
                fprintf(stderr, "Failed to save image for frame %d\n", frame);
                freeRawImage(image);
                exit(1);
            }

            freeRawImage(image);
            exit(0);
        } else {
            // Parent process: track active processes
            printf("Parent process, active_processes: %d\n", active_processes);
            active_processes++;
        }

        // Update the scale and position for the next frame
        current_scale *= scale_step;
        current_x += x_step;
        current_y += y_step;

        // Wait for a process to finish if needed
        if (active_processes >= num_processes) {
            wait(NULL);
            active_processes--;
        }
    }

    // Wait for any remaining child processes to finish
    while (active_processes > 0) {
        wait(NULL);
        active_processes--;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Generation completed in %.9f seconds.\n", elapsed_time);

    return 0;
}
