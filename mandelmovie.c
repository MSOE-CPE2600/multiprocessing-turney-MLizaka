/**
 * Andrew Lizak
 * FILE: mandlemovie.c
 * Date: 11/25/2024
 * LAB 11
 */


#include <stdio.h>   
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/types.h> 
#include <sys/wait.h> 
#include <math.h>    
#include <time.h>    

// Constants for the animation
#define FRAMES 100         // Total number of frames to generate
#define INITIAL_SCALE 4.0  // Initial zoom scale for the Mandelbrot set
#define FINAL_SCALE 0.0001 // Final zoom scale (very close-up)
#define INITIAL_X -0.7463  // Starting x-coordinate of zoom
#define INITIAL_Y 0.1102   // Starting y-coordinate of zoom
#define FINAL_X -0.7463    // Final x-coordinate (unchanged for this zoom)
#define FINAL_Y 0.1102     // Final y-coordinate (unchanged for this zoom)

// Function to generate a single frame of the Mandelbrot set
void generate_frame(int frame, double scale, double x_center, double y_center, int width, int height, const char *out) {
    // Prepare command-line arguments 
    char scale_arg[50], x_arg[50], y_arg[50], width_arg[50], height_arg[50];
    snprintf(scale_arg, sizeof(scale_arg), "-s %lf", scale);  // Scale argument
    snprintf(x_arg, sizeof(x_arg), "-x %lf", x_center);       // X-coordinate argument
    snprintf(y_arg, sizeof(y_arg), "-y %lf", y_center);       // Y-coordinate argument
    snprintf(width_arg, sizeof(width_arg), "-W %d", width);  // Width of the image
    snprintf(height_arg, sizeof(height_arg), "-H %d", height); // Height of the image

    // Execute the Mandelbrot gen
    execlp("./mandel", "mandel", scale_arg, x_arg, y_arg, width_arg, height_arg, "-o", out, (char *)NULL);
    
    // If execlp fails, output an error message and exit
    perror("Error running mandel program");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Check if the user provided the correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "enter number of processes only!");
        exit(1);
    }

    // Parse the number of processes from the command-line argument
    int num_processes = atoi(argv[1]);
    if (num_processes < 1 || num_processes > FRAMES) {
        fprintf(stderr, "Number must be between 1 and %d\n", FRAMES);
        exit(1);
    }

    // Calculate the step sizes 
    double scale_step = pow(FINAL_SCALE / INITIAL_SCALE, 1.0 / FRAMES); // zoom
    double x_step = (FINAL_X - INITIAL_X) / FRAMES; // Change in x-coordinate per frame
    double y_step = (FINAL_Y - INITIAL_Y) / FRAMES; // Change in y-coordinate per frame

    // first frame vals
    double current_scale = INITIAL_SCALE;
    double current_x = INITIAL_X;
    double current_y = INITIAL_Y;

    // Timer strt
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start); 

    int active_processes = 0; // Tracking the number of active child processes

    // Loop to generate all frames
    for (int frame = 0; frame < FRAMES; frame++) {
        char outfile[50];
        snprintf(outfile, sizeof(outfile), "frame%03d.jpg", frame + 1); // Create output filename

        // If we've reached the max number of processes, wait for one to finish
        if (active_processes >= num_processes) {
            wait(NULL); // Wait for any child process to complete
            active_processes--; // Decrement active process count
        }

        // Fork a new process to make frame
        pid_t pid = fork();
        if (pid < 0) {
            perror("Error during fork");
            exit(1); // Exit if fork fails
        } else if (pid == 0) {
            // Child process: generate the frame
            generate_frame(frame, current_scale, current_x, current_y, 1000, 1000, outfile);
        } else {
            // Parent process: increment the active process count
            active_processes++;
        }

        // val for next frame
        current_scale *= scale_step; // Zoom in
        current_x += x_step;         // Update x-coordinate
        current_y += y_step;         // Update y-coordinate
    }

    // dont abandon your children 
    while (active_processes > 0) {
        wait(NULL);
        active_processes--;
    }

    // stop timer and print result 
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Completed in %.9f seconds.\n", elapsed_time);

    return 0; //the end
}
