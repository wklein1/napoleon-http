#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../include/filesystem/filesystem.h"
#include "../../include/reader.h"

struct posix_file {
    struct fs_file base;
    int fd;
};

static int resolve_under_root(struct fs *vfs, const char *path, char *real_path_out){
	return FS_OK;
}

static ssize_t posix_read(struct fs_file *file, void *buffer, size_t cap) {
    if (!file || !buffer) return -1;
    struct posix_file *pf = (struct posix_file*)file;
    if (pf->fd < 0) return -1;
    if (cap <= 0) return -1;
    return read_some(pf->fd, buffer, cap);
}

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

static int posix_close(struct fs_file *file) {
    if (!file) return FS_INVALID;
    struct posix_file *pf = (struct posix_file*)file;
	int ret = close(pf->fd);
    pf->fd = -1;
    free(pf);
    return ret;
}

static const struct fs_file_ops posix_file_ops = {
    .read  = posix_read,
    .seek  = posix_seek,
    .close = posix_close,
};

static int posix_stat(struct fs *vfs, const char *path, struct fs_stat *stat_out){
	if(!vfs || !path || !stat_out) return FS_INVALID;

	char* real_path;
	int ret = resolve_under_root(vfs, path, real_path);
	if(ret != FS_OK) return ret;

	struct stat stat;
	if(lstat(real_path, &stat)<0) return FS_ERROR;
	stat_out->size = (stat.st_size < 0) ? 0 : (uint64_t)stat.st_size;
	if(S_ISREG(stat.st_mode)){
		stat_out->node_type = FS_NODE_FILE;
	}else if(S_ISDIR(stat.st_mode)){
		stat_out->node_type = FS_NODE_DIR;
	}
	else{
		stat_out->node_type = FS_NODE_UNKNOWN;
	}
	return FS_OK;
}

static int posix_open(struct fs *vfs, const char *path, struct fs_file **file_out){
	if (!vfs || !vfs->ctx || !path || !file_out) return FS_INVALID;

	char* real_path;
	int ret = resolve_under_root(vfs, path, real_path);
	if(ret != FS_OK) return ret;

  	int fd = open(real_path, O_RDONLY | O_CLOEXEC);
  	if (fd < 0) {
    	if (errno == ENOENT) return FS_NOT_FOUND;
    	return FS_ERROR;
  	}

	struct posix_file *pf = calloc(1,sizeof(struct posix_file));
	if(!pf){
		close(fd);
		return FS_ERROR;
	}

	pf->base.ops = &posix_file_ops;
	pf->fd = fd;

	*file_out = &pf->base;

	return FS_OK;
}

static const struct fs_ops posix_fs_ops = {
    .stat = posix_stat,
    .open = posix_open,
};
