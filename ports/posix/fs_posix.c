#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../include/filesystem/filesystem.h"
#include "../../include/reader.h"

struct posix_file {
    struct fs_file base;
    int fd;
};

/**
 * @brief Build an absolute filesystem path under @p vfs->root and forbid ".." traversal.
 *
 * Strips a leading '/' from @p path, rejects any path that contains a "/.."
 * component or a ".." component at the beginning, and then joins
 * @p vfs->root with the (now relative) @p path. The resulting string is
 * heap-allocated and returned via @p real_path_out.
 *
 * Ownership: the caller owns *@p real_path_out and must free() it.
 *
 * @param vfs            Filesystem instance (must have a valid root).
 * @param path           Input path (may start with '/', not NULL).
 * @param real_path_out  [out] On success, set to a newly-allocated full path.
 *
 * @return FS_OK on success;
 *         FS_INVALID on bad arguments or traversal attempt;
 *         FS_ERROR on allocation/formatting failure.
 */
static int resolve_under_root(struct fs *vfs, const char *path, char **real_path_out){
	if(!vfs || !path || !real_path_out){
		return FS_INVALID;
	}
	if(path[0]=='/') path++;
	size_t path_len = strlen(path);

	if(path_len >= 2){
		if (strncmp(path, "..", 2) == 0) return FS_INVALID;
		for(size_t i=0; i+2<path_len; i++){
			if(path[i] == '/' && path[i+1] == '.' && path[i+2] == '.'){
				return FS_INVALID;
			}
		}
	}
	bool need_slash = (vfs->root_len > 0 && vfs->root[vfs->root_len-1] != '/');
	size_t real_path_len = path_len + (size_t)need_slash + vfs->root_len;
	char *real_path = calloc(real_path_len+1, sizeof(char));
	if(!real_path) return FS_ERROR;
	int written = 0;
	if(need_slash){
		written = snprintf(real_path, real_path_len+1, "%s/%s", vfs->root, path);
	}else{
		written = snprintf(real_path, real_path_len+1, "%s%s", vfs->root, path);
	}
	if(written < 0 || (size_t)written >= real_path_len+1){
		free(real_path);
		return FS_ERROR;
	}
	*real_path_out = real_path;
	return FS_OK;
}


/**
 * @brief Read up to @p cap bytes from an open file descriptor.
 *
 * Thin wrapper around read(2) using the read_some() helper to handle EINTR.
 * A @p cap of 0 is treated as a successful no-op and returns 0.
 *
 * @param file    File handle returned by posix_open (non-NULL).
 * @param buffer  Destination buffer (non-NULL).
 * @param cap     Maximum number of bytes to read (0 allowed).
 *
 * @return Number of bytes read (>=0) on success;
 *         -1 on error (invalid args, closed fd, or read failure).
 */
static ssize_t posix_read_some(struct fs_file *file, void *buffer, size_t cap) {
    if (!file || !buffer) return -1;
    struct posix_file *pf = (struct posix_file*)file;
    if (pf->fd < 0) return -1;
	if(cap == 0) return 0;
    return read_some(pf->fd, buffer, cap);
}

/**
 * @brief Read (attempt to) exactly @p cap bytes from an open file descriptor.
 *
 * Thin wrapper around the helper @c read_all() that repeatedly calls read(2)
 * until either @p cap bytes have been placed into @p buffer, EOF is reached,
 * or a non-recoverable error occurs. Short reads and EINTR are handled
 * internally by @c read_all().
 *
 * A @p cap of 0 is treated as a successful no-op and returns 0.
 *
 * @param file    File handle previously returned by posix_open (must be non-NULL).
 * @param buffer  Destination buffer with capacity for at least @p cap bytes (non-NULL).
 * @param cap     Target number of bytes to read (0 allowed).
 *
 * @return Number of bytes read (>= 0) on success. This may be less than @p cap
 *         only if EOF is encountered before @p cap bytes could be read;
 *         -1 on error (invalid arguments, closed/invalid fd, or read failure).
 */
static ssize_t posix_read_all(struct fs_file *file, void *buffer, size_t cap) {
    if (!file || !buffer) return -1;
    struct posix_file *pf = (struct posix_file*)file;
    if (pf->fd < 0) return -1;
	if(cap == 0) return 0;
    return read_all(pf->fd, buffer, cap);
}

/**
 * @brief Reposition file offset to an absolute @p offset (SEEK_SET).
 *
 * Converts @p offset to off_t and verifies it fits on this platform.
 * Retries lseek(2) on EINTR.
 *
 * @param file    File handle (non-NULL).
 * @param offset  New absolute offset from the beginning of the file.
 *
 * @return FS_OK on success;
 *         FS_NOT_SUPPORTED if @p offset does not fit into off_t;
 *         FS_ERROR on lseek() failure;
 *         FS_INVALID on bad arguments.
 */
static int posix_seek(struct fs_file *file, uint64_t offset){
	if(!file) return FS_INVALID;
	
    struct posix_file *pf = (struct posix_file*)file;
    if (pf->fd < 0) return -1;

	off_t off = (off_t)offset;
	if ((uint64_t)off != offset) return FS_NOT_SUPPORTED;

	while (1) {
        off_t ret = lseek(pf->fd, off, SEEK_SET);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return FS_ERROR;
        }
        return FS_OK;
    }
}


/**
 * @brief Close an open file and free the posix file struct.
 *
 * Calls close(2) on the underlying fd, then frees the @c struct posix_file.
 * After return, @p file must not be used again.
 *
 * @param file  File handle to close (non-NULL).
 *
 * @return The return value of close(2) (0 on success, -1 on error),
 *         or FS_INVALID if @p file is NULL.
 */
static int posix_close(struct fs_file *file) {
    if (!file) return FS_INVALID;
    struct posix_file *pf = (struct posix_file*)file;
	int ret = close(pf->fd);
    pf->fd = -1;
    free(pf);
    return ret;
}


/**
 * @brief Table of file operations used by the POSIX implementation fs_file handles.
 *
 * Installed into @ref posix_file::base.ops and consumed by the generic
 * filesystem wrappers (fs_read/fs_seek/fs_close).
 */
static const struct fs_file_ops posix_file_ops = {
    .read_some  = posix_read_some,
    .read_all   = posix_read_all,
    .seek  		= posix_seek,
    .close 		= posix_close,
};


/**
 * @brief Query file metadata with lstat(2) under the configured root.
 *
 * Resolves @p path under @p vfs->root, then calls lstat() and maps the
 * result into @ref fs_stat (size and node type).
 *
 * @param vfs       Filesystem instance (non-NULL).
 * @param path      Path relative to root (leading '/' is allowed).
 * @param stat_out  [out] Destination for metadata (non-NULL).
 *
 * @return FS_OK on success;
 *         FS_NOT_FOUND if the path does not exist (ENOENT/ENOTDIR);
 *         FS_ERROR on other lstat() errors;
 *         FS_INVALID on bad arguments or path traversal rejection.
 */
static int posix_stat(struct fs *vfs, const char *path, struct fs_stat *stat_out){
	if(!vfs || !path || !stat_out) return FS_INVALID;
	char* real_path;
	int ret = resolve_under_root(vfs, path, &real_path);
	if(ret != FS_OK){
		return ret;
	}

	struct stat s_stat;
	int lstat_ret = 0;
    lstat_ret = lstat(real_path, &s_stat);
	if(lstat_ret < 0){
		if(errno == ENOENT || errno == ENOTDIR) return FS_NOT_FOUND;
		free(real_path);
		return FS_ERROR;
	}

	stat_out->size = (s_stat.st_size < 0) ? 0 : (uint64_t)s_stat.st_size;

	if(S_ISREG(s_stat.st_mode)){
		stat_out->node_type = FS_NODE_FILE;
	}else if(S_ISDIR(s_stat.st_mode)){
		stat_out->node_type = FS_NODE_DIR;
	}
	else{
		stat_out->node_type = FS_NODE_UNKNOWN;
	}
	free(real_path);
	return FS_OK;
}


/**
 * @brief Open a regular file for reading under the configured root.
 *
 * Resolves @p path under @p vfs->root, opens it read-only (O_RDONLY | O_CLOEXEC),
 * retries on EINTR, and allocates a @c struct posix_file wrapper. Directories
 * should be rejected by the caller if needed (or via fstat after open).
 *
 * On success, *@p file_out is set to the embedded @ref fs_file base pointer.
 *
 * Ownership: the caller is responsible for calling fs_close() on the returned
 * file to release both the fd and the wrapper.
 *
 * @param vfs       Filesystem instance (non-NULL).
 * @param path      Path relative to root (leading '/' is allowed).
 * @param file_out  [out] Receives the opened file handle (non-NULL).
 *
 * @return FS_OK on success;
 *         FS_NOT_FOUND if the path does not exist;
 *         FS_ERROR on open/alloc failures;
 *         FS_INVALID on bad arguments or path traversal rejection.
 */
static int posix_open(struct fs *vfs, const char *path, struct fs_file **file_out){
	if (!vfs || !path || !file_out) return FS_INVALID;

	char* real_path;
	int ret = resolve_under_root(vfs, path, &real_path);
	if(ret != FS_OK) return ret;

	int fd = -1;
	while(1){
  		fd = open(real_path, O_RDONLY | O_CLOEXEC);
  		if (fd < 0) {
			if(errno == EINTR) continue;
			free(real_path);
    		if(errno == ENOENT) return FS_NOT_FOUND;
    		return FS_ERROR;
  		}else{
			break;
		}
	}

	struct posix_file *pf = calloc(1,sizeof(struct posix_file));
	if(!pf){
		close(fd);
		free(real_path);
		return FS_ERROR;
	}

	pf->base.ops = &posix_file_ops;
	pf->fd = fd;

	*file_out = &pf->base;
	free(real_path);
	return FS_OK;
}


/**
 * @brief Create a directory relative to the configured filesystem root.
 *
 * Resolves @p path under @p vfs->root (rejecting any traversal outside the root)
 * and calls mkdir(2) with mode @c 0755. When @p recursive is true, this behaves
 * like @c mkdir -p: all missing parent components are created in order and
 * existing components (EEXIST) are treated as success. EINTR is retried.
 *
 * When @p recursive is false, only the leaf directory is created; EEXIST is
 * treated as success.
 *
 * @param vfs       Filesystem instance (must be non-NULL and initialized).
 * @param path      Directory path relative to the root (a leading '/' is allowed).
 * @param recursive If true, create missing parents (mkdir -p semantics);
 *                  if false, create only the leaf directory.
 *
 * @return FS_OK        on success (including when the directory already exists).
 * 		   FS_INVALID   on bad arguments or when path resolution rejects traversal.
 * 		   FS_ERROR     on other failures from resolve_under_root() or mkdir(2).
 *
 * @note Path resolution is performed by resolve_under_root(), which prevents
 *       ".." traversal from escaping @p vfs->root.
 */ 
static int posix_mkdir(struct fs *vfs, const char *path, bool recursive){
    char *real_path = NULL;
    int ret = resolve_under_root(vfs, path, &real_path);
    if (ret != FS_OK) return ret;

    ret = FS_OK;
    if (!recursive) {
        while (1) {
			ret = mkdir(real_path, 0755);
            if(ret == 0) break; 
            if(ret < 0 && errno == EEXIST) break;
            if(ret < 0 && errno == EINTR) continue;
            ret = FS_ERROR; break;
        }
        free(real_path);
        return ret;
    }

    size_t real_path_len = strlen(real_path);
    for (size_t i = 1; i < real_path_len; i++) {
        if (real_path[i] != '/' && real_path[i] != '\0'){
			continue;
		}
        char c = real_path[i];
        real_path[i] = '\0';
        while (1) {
			ret = mkdir(real_path, 0755);
            if(ret == 0) break; 
            if(ret < 0 && errno == EEXIST) break;
            if(ret < 0 && errno == EINTR) continue;
            ret = FS_ERROR; goto cleanup;
        }
        real_path[i] = c;
    }
cleanup:
    free(real_path);
    return ret;
}

/**
 * @brief POSIX filesystem operations table for the filesystem abstraction.
 *
 * Implements the @ref fs_ops interface using POSIX syscalls.
 * Currently provides:
 *  - `stat` via `lstat(2)` (path resolved under the configured root)
 *  - `open` via `open(2)` with `O_RDONLY | O_CLOEXEC`
 *
 * Path resolution is constrained to the configured root (see
 * resolve_under_root) to mitigate directory traversal.
 *
 * @note This object has internal linkage (`static`) and immutable contents.
 *       Obtain a pointer with get_fs_ops(). The pointer is valid for the
 *       entire program lifetime and may be shared across threads.
 */
static const struct fs_ops posix_fs_ops = {
    .stat = posix_stat,
    .open = posix_open,
	.mkdir = posix_mkdir,
};

const struct fs_ops* get_fs_ops(){
	return &posix_fs_ops;
}
