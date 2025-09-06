#include "../../include/filesystem/filesystem.h"

int fs_init(struct fs *vfs, const struct fs_ops *ops, const char *root, void *ctx)
{
    if (!vfs || !ops || !root)		return FS_INVALID;
    if (!ops->stat || !ops->open)	return FS_INVALID;

    vfs->ops  = ops;
    vfs->root = root;
    vfs->ctx  = ctx;
    return FS_OK;
}


int fs_stat(struct fs *vfs, const char *path, struct fs_stat *stat_out)
{
    if (!vfs || !vfs->ops)		return FS_INVALID;
    if (!vfs->ops->stat)		return FS_NOT_SUPPORTED;
    if (!stat_out)				return FS_INVALID;
    if (!path)                  return FS_INVALID;

    return vfs->ops->stat(vfs, path, stat_out);
}


int fs_open(struct fs *vfs, const char *path, struct fs_file **file_out)
{
    if (!vfs || !vfs->ops)		return FS_INVALID;
    if (!vfs->ops->open)		return FS_NOT_SUPPORTED;
    if (!file_out)				return FS_INVALID;
    if (!path)					return FS_INVALID;

    return vfs->ops->open(vfs, path, file_out);
}


ssize_t fs_read_some(struct fs_file *file, void *buffer, size_t cap)
{
    if (!file || !file->ops)	return FS_ERROR;
	if (!file->ops->read_some)		return FS_ERROR;
    if (cap <= 0)				return FS_ERROR;
	if (!buffer)				return FS_ERROR;

    return file->ops->read_some(file, buffer, cap);
}


ssize_t fs_read_all(struct fs_file *file, void *buffer, size_t cap)
{
    if (!file || !file->ops)	return FS_ERROR;
	if (!file->ops->read_all)	return FS_ERROR;
    if (cap <= 0)				return FS_ERROR;
	if (!buffer)				return FS_ERROR;

    return file->ops->read_all(file, buffer, cap);
}


int fs_seek(struct fs_file *file, uint64_t offset)
{
    if (!file || !file->ops)		return FS_INVALID;
    if (!file->ops->seek)			return FS_NOT_SUPPORTED;

    return file->ops->seek(file, offset);
}


int fs_close(struct fs_file *file)
{
    if (!file || !file->ops)		return FS_INVALID;
    if (!file->ops->close)			return FS_NOT_SUPPORTED;

    return file->ops->close(file);
}
