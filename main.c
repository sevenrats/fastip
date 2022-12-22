#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <curl/curl.h>
#include "util.h"

int main(int argc, char *argv[]) {
    {
	CURL *http_handle;
	CURL *http_handle2;
	CURL *http_handle3;
	CURLM *multi_handle;
 
	int still_running = 1; /* keep number of running handles */
 
	http_handle = curl_easy_init();
	http_handle2 = curl_easy_init();
	http_handle3 = curl_easy_init();

	curl_easy_setopt(http_handle, CURLOPT_URL, "https://ipv4.icanhazip.com/");
	curl_easy_setopt(http_handle2, CURLOPT_URL, "https://ident.me");
    curl_easy_setopt(http_handle3, CURLOPT_URL, "https://checkip.amazonaws.com/");
 
	/* init a multi stack */
	multi_handle = curl_multi_init();
 
	/* add the individual transfers */
	curl_multi_add_handle(multi_handle, http_handle);
	curl_multi_add_handle(multi_handle, http_handle2);
	curl_multi_add_handle(multi_handle, http_handle3);
 
	while(still_running) {
		CURLMsg *msg;
		int queued;
		CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
 
		if(still_running)
			/* wait for activity, timeout or "nothing" */
            
			mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);
 
		if(mc)
			break;

		do {
			msg = curl_multi_info_read(multi_handle, &queued);
			if(msg && msg->msg == CURLMSG_DONE) {
				if(msg->data.result == CURLE_OK) {
                    still_running = 0;
				}
			}
		} while(msg);
	}
 
	curl_multi_remove_handle(multi_handle, http_handle);
	curl_multi_remove_handle(multi_handle, http_handle2);
	curl_multi_remove_handle(multi_handle, http_handle3);

 
	curl_multi_cleanup(multi_handle);
 
	curl_easy_cleanup(http_handle);
	curl_easy_cleanup(http_handle2);
	curl_easy_cleanup(http_handle3);
   
	return 0;
}
}