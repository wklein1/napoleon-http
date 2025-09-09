#ifndef READER_H
#define READER_H

#include <unistd.h>

/**
 * @file reader.h
 * @brief Utility functions for safe and reliable reading from file descriptors.
 *
 * These helpers wrap the POSIX `read(2)` system call to simplify common
 * I/O patterns, including handling of `EINTR` and ensuring complete reads
 * for a fixed-length buffer.
 */

/**
 * @brief Attempt to read up to @p len bytes from a file descriptor.
 *
 * This is a thin wrapper around `read(2)` that transparently retries if the
 * call is interrupted by a signal (`errno == EINTR`).  
 *
 * Unlike @ref read_all, this function may return fewer than @p len bytes
 * even if more data is available, depending on the underlying stream and
 * buffering.
 *
 * @param fd 		File descriptor to read from (e.g., a socket or file).
 * @param buffer 	Destination buffer to store the data (must be at least @p len bytes).
 * @param len 		Maximum number of bytes to attempt to read.
 *
 * @return
 *   - Number of bytes actually read (≥0).  
 *   - `0` if end-of-file (EOF) is reached.  
 *   - `-1` on error (with `errno` set).
 */
ssize_t read_some(int fd, void *buffer, size_t len);


/**
 * @brief Attempt to read exactly @p len bytes from a file descriptor.
 *
 * Internally calls @ref read_some in a loop until either the requested number
 * of bytes has been read, EOF is reached, or an error occurs.  
 *
 * If EOF occurs before @p len bytes could be read, the function returns the
 * number of bytes actually read (which may be less than @p len).
 *
 * @param fd 		File descriptor to read from (e.g., a socket or file).
 * @param buffer 	Destination buffer to store the data (must be at least @p len bytes).
 * @param len 		Number of bytes to read in total.
 *
 * @return
 *   - Number of bytes actually read (may be less than @p len if EOF reached).  
 *   - `-1` on error (with `errno` set).
 */
ssize_t read_all(int fd, void *buffer, size_t len);


#endif /* READER_H */
