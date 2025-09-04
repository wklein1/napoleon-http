#ifndef FS_POSIX_H
#define FS_POSIX_H

/**
 * @brief Return the POSIX @ref fs_ops vtable.
 *
 * @return Pointer to a statically allocated, immutable operations table.
 *         The pointer remains valid for the entire program lifetime.
 */
const struct fs_ops* get_fs_ops();

#endif /* FS_POSIX_H */
