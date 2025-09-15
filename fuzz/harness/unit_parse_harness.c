#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "../../include/http/http_parser.h"
#include "../../include/http/http_request.h"
#include "../../include/reader.h"


static int read_all_from_stdin(uint8_t **buffer_out, size_t *buff_out_len){
    const size_t chunk_size = 2048;
    uint8_t *buffer = NULL;
    size_t limit = chunk_size;
	size_t total_read = 0;
    uint8_t tmp[chunk_size];

    while(1){
        ssize_t curr_read = read_some(STDIN_FILENO, tmp, chunk_size);
        if(curr_read < 0){ 
			free(buffer); 
			return -1;
		}
        if(curr_read == 0) break;

		if((size_t)curr_read > SIZE_MAX - total_read){
			free(buffer);
			return -1;
		}

        if(buffer == NULL){
            buffer = calloc(1,limit);
            if (!buffer) return -1;
        }
        if(total_read + (size_t)curr_read > limit){
            while(limit < total_read + (size_t)curr_read){
                if(limit > SIZE_MAX / 2){
					free(buffer);
					return -1;
				}
				limit *= 2;
			}

            uint8_t *new_buffer = realloc(buffer, limit);
            if(!new_buffer){
				free(buffer);
				return -1;
			}
            buffer = new_buffer; 
        }
        memcpy(buffer + total_read, tmp, (size_t)curr_read);
        total_read += (size_t)curr_read;
    }
    *buffer_out = buffer; *buff_out_len = total_read;
    return 0;
}

static void fuzz(const uint8_t *data, size_t size){
    if(!data || size == 0) return;

    struct http_request req = {0};
    http_request_init(&req);

    int req_line_end = http_parse_request_line((const char*)data, size, &req);

    if(req_line_end >= 0 && (size_t)req_line_end < size){
        int dropped = 0;
        http_parse_request_headers((const char*)data + req_line_end,
                                         size - (size_t)req_line_end,
                                         &dropped, &req);
	}
		http_request_clear(&req);

        int dropped2 = 0;
        http_parse_request_headers((const char*)data, size, &dropped2, &req);

		http_request_clear(&req);
}


#ifdef __AFL_HAVE_MANUAL_CONTROL
int main(void) {
    __AFL_FUZZ_INIT();

    while (__AFL_LOOP(1000)) {
        uint8_t *buf = __AFL_FUZZ_TESTCASE_BUF;
        size_t   len = (size_t)__AFL_FUZZ_TESTCASE_LEN;
        if (len > 0) fuzz(buf, len);
    }
    return 0;
}
#else
int main(void) {
    uint8_t *data = NULL;
    size_t   size = 0;
    if (read_all_from_stdin(&data, &size) != 0) return 1;
    fuzz(data, size);
    free(data);
    return 0;
}
#endif
