#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <cstddef>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

enum fs_return_codes {
    FS_OK				=  0,
    FS_ERROR			= -1,
    FS_INVALID  		= -2,
    FS_NOT_SUPPORTED 	= -3,
    FS_NOT_FOUND 		= -4,
};


struct fs;
struct fs_file;

enum fs_node_type {
    FS_NODE_UNKNOWN = 0,
    FS_NODE_FILE,
    FS_NODE_DIR,
};

struct fs_stat {
    uint64_t size;
	enum fs_node_type node_type;
};


struct fs_file_ops {
    ssize_t (*read)(struct fs_file *file, void *buffer, size_t cap);
    int     (*seek)(struct fs_file *file, uint64_t offset);
    int     (*close)(struct fs_file *file);
};


struct fs_file {
    const struct fs_file_ops *ops;
};


struct fs_ops {
    int     (*stat)(struct fs *vfs, const char *path, struct fs_stat *stat_out);
    int     (*open)(struct fs *vfs, const char *path, struct fs_file **file_out);
};


struct fs {
    const struct fs_ops *ops;
	const char *root;
	size_t root_len;
    void *ctx;
};


int     fs_stat (struct fs *vfs, const char *path, struct fs_stat *stat_out);
int     fs_open (struct fs *vfs, const char *path, struct fs_file **file_out);
ssize_t fs_read (struct fs_file *file, void *buffer, size_t cap);
int     fs_seek (struct fs_file *file, uint64_t offset);
int     fs_close(struct fs_file *file);

#endif /* FILESYSTEM_H */
