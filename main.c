#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <curl/curl.h>

int main(int argc, char *argv[]) 
{
    const char *interface_name = NULL;

    // Check if an interface name is provided as an argument
    if (argc > 1) {
        interface_name = argv[1];
    }

    CURL *handles[4];
    CURLM *multi_handle;
    const char *urls[] = {
        "https://ipv4.icanhazip.com/",
        "https://ident.me",
        "https://checkip.amazonaws.com/",
        "https://domains.google.com/checkip"
    };

    int still_running = 1;
    int num_handles = sizeof(urls) / sizeof(urls[0]);

    /* Initialize CURL handles and set options */
    for (int i = 0; i < num_handles; i++) {
        handles[i] = curl_easy_init();
        if (!handles[i]) {
            fprintf(stderr, "Failed to initialize CURL handle\n");
            return EXIT_FAILURE;
        }
        curl_easy_setopt(handles[i], CURLOPT_URL, urls[i]);
        curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, 10L); // Set a timeout

        // Set the interface if specified
        if (interface_name) {
            curl_easy_setopt(handles[i], CURLOPT_INTERFACE, interface_name);
        }
    }

    /* Init a multi stack */
    multi_handle = curl_multi_init();
    if (!multi_handle) {
        fprintf(stderr, "Failed to initialize CURLM handle\n");
        return EXIT_FAILURE;
    }

    /* Add individual transfers to the multi handle */
    for (int i = 0; i < num_handles; i++) {
        curl_multi_add_handle(multi_handle, handles[i]);
    }

    /* Perform transfers */
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

        /* Check for completed requests */
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

    /* Cleanup */
    for (int i = 0; i < num_handles; i++) {
        curl_multi_remove_handle(multi_handle, handles[i]);
        curl_easy_cleanup(handles[i]);
    }
    curl_multi_cleanup(multi_handle);

    return EXIT_SUCCESS;
}
