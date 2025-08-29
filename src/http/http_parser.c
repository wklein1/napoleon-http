#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/http/http_parser.h"
#include "../../include/http/http_common.h"
#include "../../include/reader.h"

static const short DEBUG_OUT = 0;

/**
 * @brief Represents a slice of a string by start and end indices.
 */
struct string_slice {
    int start_idx;  /**< Inclusive start index */
    int end_idx;    /**< Inclusive end index */
};

/**
 * @brief Trim leading and trailing whitespaces and tabs from a string.
 *
 * @param string	Input string buffer (not null-terminated).
 * @param len		Length of the input buffer.
 *
 * @return struct string_slice		Indices of trimmed substring (start, end).
 *         Returns {0,0} if input is invalid.
 */
static struct string_slice trim_whitespace_and_tabs(const char *string, size_t len){
	struct string_slice zero_slice = {0};
	if(!string || len<2) return zero_slice;

	size_t left = strspn(string, " \t");
    size_t right = len-1;
    while (right > left && (string[right] == ' ' || string[right] == '\t')) {
        right--;
    }

	struct string_slice slice = {.start_idx=left, .end_idx=right};

	return slice;
}

/**
 * @brief Find the inclusive end index of a token, delimited by a character or '\0'.
 *
 * @param string 		Input string buffer.
 * @param len 			Length of the buffer.
 * @param delimiter 	Delimiter character.
 *
 * @return size_t Index of the last character before the delimiter.
 *         If delimiter not found, returns len-1.
 */
static size_t find_token_end(const char *string, size_t len, char delimiter){
	for(size_t i=0; i<len; i++){
		if(string[i] == delimiter || string[i] == '\0'){
			if(i>0){
				return i-1;
			}else{
				return 0;
			}
		}
	}
	 return len-1;
}

/**
 * @brief Find the position of CRLF ("\r\n") in a string buffer.
 *
 * @param string 	Input string buffer.
 * @param len 		Length of the buffer.
 *
 * @return int 		Index of CR or -1 if not found.
 */
static int find_crlf(const char *string, size_t len){
    if (!string) {
		return -1;
	}
	for (size_t i = 0; i + 1 < len; i++){
        if (string[i]=='\r' && string[i+1]=='\n'){
			return i;
		}
	}
	return -1;
}


/**
 * @brief Find the position of double CRLF ("\r\n\r\n") in a string buffer.
 *
 * @param string 	Input string buffer.
 * @param len 		Length of the buffer.
 *
 * @return int 		Index of first CR or -1 if not found.
 */
static int find_double_crlf(const char *string, size_t len){
    if (!string) {
		return -1;
	}
	for (size_t i = 0; i + 3 < len; i++){
        if (string[i]=='\r' && string[i+1]=='\n' && string[i+2]=='\r' && string[i+3]=='\n'){
			return i;
		}
	}
    return -1;
}

/**
 * @brief Read from a file descriptor until double CRLF ("\r\n\r\n") is encountered.
 *
 * @param fd File 				descriptor to read from.
 * @param buffer 				Pointer to buffer pointer (gets reallocated if needed).
 * @param buffer_len 			Initial buffer length.
 * @param new_buff_max 			Maximum allowed buffer size.
 * @param new_buff_len [out] 	Final allocated buffer size.
 * @param total_read [out] 		Number of bytes read.
 *
 * @return int 					Index of "\r\n\r\n" in buffer, or -1 on error.
 */
static int read_until_double_crlf(
    int fd,
    void **buffer,
	size_t buffer_len,
    size_t new_buff_max,
	size_t *new_buff_len,
    size_t *total_read
){
	if(!buffer || !new_buff_len || !total_read || buffer_len<1 || new_buff_max <1) return -1;

	ssize_t currently_read = 0;
	size_t chunk_size = buffer_len;
	int chunk_count = 1;

	while(1){

		currently_read = read_some(fd, (char*)(*buffer) + *total_read, chunk_size*chunk_count - *total_read);
		if(currently_read<0){
			return -1;
		}
		*total_read += currently_read;

		int idx = find_double_crlf((const char*)*buffer, *total_read);
		if(idx == -1){
			chunk_count++;

			if(chunk_size*chunk_count <= new_buff_max){
				void *tmp = realloc(*buffer, chunk_size*chunk_count);
				if(!tmp){
					return -1;
				}
				*buffer = tmp;
			}else{
				return -1;
			}
			continue;
		}
		*new_buff_len = chunk_size * chunk_count;
		return idx;
	}
}


int read_body(int fd, char **buffer, size_t buffer_len, size_t headers_end, 
			  size_t content_len, size_t max_body, size_t already_read, struct http_request *req){

	if(!buffer){
		return -1;
	}
	if(content_len == 0){
		return 0;
	} 

	size_t already_read_body = already_read - headers_end-4;

	size_t to_read = content_len <= max_body 
		? content_len - already_read_body 
		: max_body - already_read_body;

	if(DEBUG_OUT){
		printf("~~~~~~~~~~~~~~~~~~~~~\n");
		printf("%s\n",*buffer);
		printf("~~~~~~~~~~~~~~~~~~~~~\n");

		printf("buff_len: %zu\n", buffer_len);
		printf("headers_end: %zu\n", headers_end);
		printf("content_len: %zu\n", content_len);
		printf("already_read: %zu\n", already_read);
		printf("already_read_body: %zu\n", already_read_body);
		printf("to_read: %zu\n", to_read);
	}

	if(already_read + to_read > buffer_len){
		char *tmp = realloc(*buffer, (already_read+to_read)*sizeof(char));
		*buffer = tmp;
	}

	int currently_read_body = read_all(fd, *buffer+already_read, to_read);
	if(currently_read_body<0){
		return -1;
	}

	size_t actual_body_len = currently_read_body+already_read_body;

	char *tmp = calloc(actual_body_len+1, sizeof(char));
	if(!tmp) return -1;
	
	memcpy(tmp,*buffer+headers_end+4,actual_body_len);
	tmp[actual_body_len]='\0';

	req->body=tmp;

	if(actual_body_len < content_len){
		fprintf(stderr,"read %zu characters into body, but content_length is %zu\n",
		 actual_body_len,content_len);
	}
	
	return currently_read_body;
}


int http_parse_request_line(const char *buffer, size_t buffer_len, struct http_request *req){
	const char *req_line_ptr = buffer;
	int req_line_end = find_crlf(req_line_ptr, buffer_len);
	if(req_line_end == -1){
		return -1;
	}

	char** req_struct_ptrs[] = {&req->method, &req->path, &req->version};
	int token_end = 0;
	int idx = 0;
	for(int i=0;i<3;i++){
		token_end = find_token_end(req_line_ptr+idx, req_line_end-idx, ' ');
		token_end++;
		char *tmp = calloc(token_end+1, sizeof(char));
		if(!tmp){
			return -1;
		}
		memcpy(tmp, req_line_ptr+idx, token_end);
		tmp[token_end]='\0';
		*req_struct_ptrs[i] = tmp;

		idx += token_end+1;
	}

	return req_line_end+1;
}


int http_parse_request_headers(const char *buffer, size_t buffer_len, int *headers_dropped, struct http_request *req){
	const char *header_start_ptr = buffer;
	int header_count=0;
	struct http_header headers_tmp[HTTP_MAX_HEADERS];

	while(1){
		const char *header_idx_ptr = header_start_ptr;
		int header_end = find_crlf(header_start_ptr, buffer_len);
		if(header_end<1){
			break;
		}

		header_start_ptr+=header_end+2;
		
		if(header_count >= HTTP_MAX_HEADERS){
			*headers_dropped+=1;
			continue;
		}

		struct http_header header = {0};	

		int idx=0;
		for(int i=0;i<2;i++){
			header_idx_ptr+=idx;
			char delimiter = i == 0 ? ':' : '\r';

			int token_end = find_token_end(header_idx_ptr, header_end+1-idx, delimiter);
			struct string_slice trimmed_slice = trim_whitespace_and_tabs(header_idx_ptr, token_end+1);
			header_idx_ptr += trimmed_slice.start_idx;
			int token_len = trimmed_slice.end_idx - trimmed_slice.start_idx +1;

			char *tmp = calloc(token_len+1, sizeof(char));
			if(!tmp){
				return -1;
			}

			memcpy(tmp,header_idx_ptr,token_len);
			tmp[token_len]='\0';

			if(i == 0){
				header.name = tmp;
			}else{
				header.value = tmp;
			}
			idx+=token_end+2;
		}
		headers_tmp[header_count]=header;
		header_count++;
	}

	struct http_header *headers = calloc(header_count, sizeof(struct http_header));
	if(!headers) return -1;

	for(int i=0;i<header_count;i++){
		headers[i].name = headers_tmp[i].name;
		headers[i].value = headers_tmp[i].value;
	}
	req->headers=headers;
	req->num_headers = header_count;
	return header_count;
}


int http_parse_request(int fd, void **buffer, size_t buffer_len, struct http_request *req){

	if(!req || !buffer || !(*buffer) || buffer_len <1){
		fprintf(stderr, "error reading request, invalid args\n");
		return -1;
	};

	size_t headers_max = HTTP_MAX_HEADERS_BUFFER;
	size_t body_max = HTTP_MAX_BODY_BUFFER;
	size_t total_read = 0;
	size_t new_buff_len = 0;
	int headers_dropped = 0;
	
	printf("\n############ Parse request ############\n");

	int headers_end = read_until_double_crlf(fd, buffer, buffer_len, headers_max, &total_read, &new_buff_len);
	if(headers_end == -1){
		fprintf(stderr, "error reading request headers\n");
		return -1;
	}

	int req_line_end = http_parse_request_line(*buffer, new_buff_len, req);
	if(req_line_end < 0){	
		fprintf(stderr, "error parsing request line\n");
		return -1;
	}


	printf("Method: %s\n", req->method);
	printf("Path: %s\n", req->path);
	printf("Version: %s\n", req->version);

	int headers_parsed = http_parse_request_headers(*buffer+req_line_end+1, new_buff_len-req_line_end-1, 
												  &headers_dropped, req);
	if(headers_parsed<0){
		fprintf(stderr, "error parsing request headers\n");
		return -1;
	}
	printf("\nheaders parsed: %zu, headers dropped: %d\n\n",req->num_headers,headers_dropped);

	size_t content_len = 0;
	char content_len_header_name[] = "Content-Length";
	for(int i=0;i<headers_parsed;i++){
		printf("%s: \"%s\"\n",req->headers[i].name,req->headers[i].value);
		if(strncmp(req->headers[i].name,content_len_header_name, strlen(content_len_header_name))==0){
			content_len=atoll(req->headers[i].value);
		}
	}

	if(content_len == 0){
		printf("########## Parse request END ##########\n\n");
		return 0;
	}

	int read_from_body = read_body(fd,(char**)buffer, new_buff_len, headers_end, content_len, body_max, total_read, req);
	if(read_from_body <0){
		fprintf(stderr, "error reading request body\n");
		return -1;
	}

	printf("\nBody: %s\n", req->body);
	
	printf("########## Parse request END ##########\n\n");

	return 0;
}
