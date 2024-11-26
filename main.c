#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>

#define MAX_REQUESTS 5
#define URL_FILE "/etc/fastip/sources.txt"

// Function to read URLs from a file and dynamically allocate an array for them
char **read_urls(const char *filename, int *url_count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    char **urls = NULL;
    char buffer[1024];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove trailing newline
        buffer[strcspn(buffer, "\r\n")] = '\0';

        // Allocate/reallocate memory for the URL array
        char **temp = realloc(urls, (count + 1) * sizeof(char *));
        if (!temp) {
            perror("Memory allocation error");
            free(urls);
            fclose(file);
            return NULL;
        }
        urls = temp;

        // Store the URL
        urls[count] = strdup(buffer);
        if (!urls[count]) {
            perror("Memory allocation error");
            for (int i = 0; i < count; i++) free(urls[i]);
            free(urls);
            fclose(file);
            return NULL;
        }

        count++;
    }

    fclose(file);
    *url_count = count;
    return urls;
}

// Function to select up to MAX_REQUESTS random URLs from the list
char **select_random_urls(char **urls, int total_urls, int *selected_count) {
    *selected_count = total_urls < MAX_REQUESTS ? total_urls : MAX_REQUESTS;

    char **selected_urls = malloc(*selected_count * sizeof(char *));
    if (!selected_urls) {
        perror("Memory allocation error");
        return NULL;
    }

    int *indices = malloc(total_urls * sizeof(int));
    for (int i = 0; i < total_urls; i++) indices[i] = i;

    srand(time(NULL)); // Seed random number generator

    for (int i = 0; i < *selected_count; i++) {
        int rand_index = rand() % (total_urls - i);
        selected_urls[i] = urls[indices[rand_index]];
        indices[rand_index] = indices[total_urls - i - 1];
    }

    free(indices);
    return selected_urls;
}

int main(int argc, char *argv[]) {
    const char *interface_name = NULL;

    // Check if a network interface is specified
    if (argc > 1) {
        interface_name = argv[1];
    }

    int num_urls = 0;
    char **urls = read_urls(URL_FILE, &num_urls);
    if (!urls || num_urls == 0) {
        fprintf(stderr, "No URLs found in the file: %s\n", URL_FILE);
        return EXIT_FAILURE;
    }

    int selected_count = 0;
    char **selected_urls = select_random_urls(urls, num_urls, &selected_count);
    if (!selected_urls) {
        fprintf(stderr, "Failed to select random URLs.\n");
        for (int i = 0; i < num_urls; i++) free(urls[i]);
        free(urls);
        return EXIT_FAILURE;
    }

    CURL **handles = malloc(selected_count * sizeof(CURL *));
    if (!handles) {
        perror("Memory allocation error");
        free(selected_urls);
        for (int i = 0; i < num_urls; i++) free(urls[i]);
        free(urls);
        return EXIT_FAILURE;
    }

    CURLM *multi_handle;
    int still_running = 1;

    // Initialize CURL handles and set options
    for (int i = 0; i < selected_count; i++) {
        handles[i] = curl_easy_init();
        if (!handles[i]) {
            fprintf(stderr, "Failed to initialize CURL handle\n");
            return EXIT_FAILURE;
        }
        curl_easy_setopt(handles[i], CURLOPT_URL, selected_urls[i]);
        curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, 10L); // Set a timeout

        // Set the interface if specified
        if (interface_name) {
            curl_easy_setopt(handles[i], CURLOPT_INTERFACE, interface_name);
        }
    }

    // Init a multi stack
    multi_handle = curl_multi_init();
    if (!multi_handle) {
        fprintf(stderr, "Failed to initialize CURLM handle\n");
        return EXIT_FAILURE;
    }

    // Add individual transfers to the multi handle
    for (int i = 0; i < selected_count; i++) {
        curl_multi_add_handle(multi_handle, handles[i]);
    }

    // Perform transfers
    while (still_running) {
        CURLMsg *msg;
        int queued;
        CURLMcode mc = curl_multi_perform(multi_handle, &still_running);

        if (still_running)
            mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

        if (mc != CURLM_OK) {
            fprintf(stderr, "CURLM error: %s\n", curl_multi_strerror(mc));
            break;
        }

        // Check for completed requests
        do {
            msg = curl_multi_info_read(multi_handle, &queued);
            if (msg && msg->msg == CURLMSG_DONE) {
                if (msg->data.result == CURLE_OK) {
                    char *effective_url;
                    curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &effective_url);

                    /* Exit early on first successful request */
                    still_running = 0;
                    break;
                }
            }
        } while (msg);
    }

    // Cleanup
    for (int i = 0; i < selected_count; i++) {
        curl_multi_remove_handle(multi_handle, handles[i]);
        curl_easy_cleanup(handles[i]);
    }
    for (int i = 0; i < num_urls; i++) {
        free(urls[i]);
    }
    free(selected_urls);
    free(handles);
    free(urls);
    curl_multi_cleanup(multi_handle);

    return EXIT_SUCCESS;
}
