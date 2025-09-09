#include <errno.h>	
#include <stddef.h>
#include <unistd.h>
#include "../include/reader.h"

ssize_t read_some(int fd, void *buffer, size_t len){

	while(1){
		ssize_t n = read(fd, buffer, len);
		if(n<0 && errno == EINTR) continue;
		return n;
	}
}


ssize_t read_all(int fd, void *buffer, size_t len){
	char *index = (char*)buffer;
	size_t total_read = 0;
	ssize_t currently_read = 0;
	while(total_read < len){
		currently_read = read_some(fd, index+total_read, len-total_read);
		if(currently_read < 0) return currently_read;
		if(currently_read == 0) break;
		total_read += currently_read;
	}

	return total_read;

}
