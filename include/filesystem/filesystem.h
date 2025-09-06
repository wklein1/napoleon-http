#ifndef FILESYSTEM_H
#define FILESYSTEM_H

/**
 * @file filesystem.h
 * @brief Small filesystem abstraction layer used by the static file router.
 *
 * The API separates a generic virtual FS handle (@ref fs) from
 * specific implementations (provided via vtables in @ref fs_ops and
 * @ref fs_file_ops). Each concrete implementation stores its own state in
 * @ref fs::ctx and may embed @ref fs_file as the first field of its
 * concrete file handle to support polymorphic calls.
 *
 * Convention in this codebase: callers pass paths *relative to* @ref fs::root.
 */


#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>


/** 
 * @enum fs_return_codes
 * @brief Common negative error codes used by the VFS.
 */
enum fs_return_codes {
    FS_OK			  =  0,  /**< Success. */
    FS_ERROR		  = -1,  /**< Generic failure. */
    FS_INVALID		  = -2,  /**< Invalid argument / bad handle. */
    FS_NOT_SUPPORTED  = -3,  /**< Operation not supported by the filesystem. */
    FS_NOT_FOUND	  = -4,  /**< Path not found. */
};


/* Forward declarations for opaque-style handles. */
struct fs;		 /**< Opaque filesystem instance (VFS). */
struct fs_file;  /**< Opaque handle for an open file. */


/**
 * @enum fs_node_type
 * @brief Kind of node addressed by a path.
 */
enum fs_node_type {
    FS_NODE_UNKNOWN = 0,	/**< Unknown or unsupported node kind. */
    FS_NODE_FILE,			/**< Regular file. */
    FS_NODE_DIR,			/**< Directory. */
};


/**
 * @brief Lightweight metadata returned by @ref fs_stat.
 *
 * Size may be 0 if unknown (e.g., streaming sources). For directories
 * size is typically 0.
 */
struct fs_stat {
    uint64_t			size;		/**< File size in bytes (0 if unknown or dir). */
    enum fs_node_type	node_type;	/**< Kind of node (file/dir/unknown). */
};


/**
 * @brief Per-open-file operations (vtable).
 *
 * Implementations provide these Functions to support reading, optional seeking,
 * and closing a concrete file handle. The first field of the concrete handle
 * should be an embedded @ref fs_file so callers can dispatch via @ref fs_file::ops.
 */
struct fs_file_ops {


	/**
     * @brief Implementation for “read up to @p cap bytes”.
     *
     * Should perform a best-effort read and may return fewer than @p cap
     * bytes (short read). Return 0 on EOF or a negative @ref fs_return_codes
     * value on error.
     */
    ssize_t (*read_some)(struct fs_file *file, void *buffer, size_t cap);

	
	/**
     * @brief Implementation for “read until @p cap bytes or EOF/error”.
     *
     * Should attempt to fill @p buffer with exactly @p cap bytes unless EOF
     * or an error occurs. On EOF before @p cap, return the number of bytes
     * actually read (>= 0). On error, return a negative @ref fs_return_codes.
     */
    ssize_t (*read_all)(struct fs_file *file, void *buffer, size_t cap);


    /**
     * @brief Move the read position to absolute @p offset (in bytes).
     * @note Optional; may be NULL if seeking is unsupported.
	 *
     * @return @ref FS_OK on success, @ref FS_NOT_SUPPORTED if NULL,
     *         or a negative error code.
     */
    int (*seek)(struct fs_file *file, uint64_t offset);


    /**
     * @brief Close the file and release resources.
	 *
     * @return @ref FS_OK on success or a negative error code.
     */
    int (*close)(struct fs_file *file);
};


/**
 * @brief Public base part of a backend-specific file handle.
 *
 * Concrete filesystem implementations embed this as the first field
 * in their concrete file struct, so calls can be dispatched via @ref fs_file::ops.
 */
struct fs_file {
    const struct fs_file_ops *ops; /**< Vtable for this file handle. */
    /* Implementation-specific fields follow in the concrete struct (not exposed here). */
};


/**
 * @brief Filesystem root operations (vtable).
 *
 * Implementations provide these functions to perform @c stat and @c open
 * relative to the filesystem’s configured root.
 */
struct fs_ops {
    /**
     * @brief Get metadata for @p path.
     * @param vfs      Filesystem handle.
     * @param path     Path to query. Convention: relative to @ref fs::root.
     * @param stat_out Output metadata (must not be NULL).
	 *
     * @return @ref FS_OK, @ref FS_NOT_FOUND, or a negative error code.
     */
    int (*stat)(struct fs *vfs, const char *path, struct fs_stat *stat_out);

    /**
     * @brief Open @p path for reading.
     * @param vfs      Filesystem handle.
     * @param path     Path to open. Convention: relative to @ref fs::root.
     * @param file_out Receives an opened file handle on success (must not be NULL).
	 *
     * @return @ref FS_OK, @ref FS_NOT_FOUND, or a negative error code.
     */
    int (*open)(struct fs *vfs, const char *path, struct fs_file **file_out);
};


/**
 * @brief Virtual filesystem handle shared by the server.
 *
 * - @ref root is a constant docroot/mount prefix and must outlive this struct.
 * - @ref ctx holds implementation-defined state (mount handle, partition pointer, etc.).
 * - @ref ops points to the function table supplied by the implementation.
 */
struct fs {
    const struct fs_ops *ops;		/**< Operation table. */
    const char			*root;		/**< Docroot/mount prefix (immutable; not owned). */
	size_t 				 root_len;  /**< Length of the root prefix in bytes */
    void				*ctx;		/**< Implementation-defined context/state. */
};


/**
 * @brief Initialize a virtual filesystem handle.
 *
 * Sets the operation table, docroot, and implementation context.
 * The strings and pointers passed in must outlive @p vfs.
 *
 * @param vfs  Target struct to initialize (must not be NULL).
 * @param ops  Operation table with at least .stat and .open set (must not be NULL).
 * @param root Docroot/mount prefix (must not be NULL; not owned by @p vfs).
 * @param ctx  Implementation-defined context (may be NULL).
 *
 * @return FS_OK on success; FS_INVALID if any required pointer is missing.
 */
int fs_init(struct fs *vfs, const struct fs_ops *ops, const char *root, void *ctx);


/**
 * @brief Fetch metadata for a path.
 * @param vfs      Filesystem handle.
 * @param path     Path to query (typically relative to @ref fs::root).
 * @param stat_out Output metadata (must not be NULL).
 *
 * @return @ref FS_OK, @ref FS_NOT_FOUND, or a negative error code.
 */
int fs_stat (struct fs *vfs, const char *path, struct fs_stat *stat_out);


/**
 * @brief Open a file for reading.
 * @param vfs      Filesystem handle.
 * @param path     Path to open (typically relative to @ref fs::root).
 * @param file_out Receives the opened file on success (must not be NULL).
 *
 * @return @ref FS_OK, @ref FS_NOT_FOUND, or a negative error code.
 */
int fs_open (struct fs *vfs, const char *path, struct fs_file **file_out);


/**
 * @brief Read at most @p cap bytes from an open file into @p buffer.
 *
 * This function may return fewer than @p cap bytes even if more data is
 * available (i.e., a short read). A return value of 0 indicates end-of-file.
 *
 * Semantics:
 *  - If @p cap == 0, the call is a no-op and returns 0.
 *  - On success, returns the number of bytes read (>= 0).
 *  - On error, returns a negative @ref fs_return_codes value.
 *
 * @param file   Open file handle (must not be NULL).
 * @param buffer Destination buffer (must not be NULL when @p cap > 0).
 * @param cap    Maximum number of bytes to read.
 *
 * @return Bytes read (>= 0), 0 on EOF, or < 0 on error. 
 */
ssize_t fs_read_some (struct fs_file *file, void *buffer, size_t cap);


/**
 * @brief Attempt to read exactly @p cap bytes unless EOF or an error occurs.
 *
 * Reads repeatedly until either @p cap bytes have been read, EOF is reached,
 * or an error occurs. If EOF occurs before @p cap bytes are read, the function
 * returns the number of bytes actually read (which may be less than @p cap).
 *
 * Semantics:
 *  - If @p cap == 0, the call is a no-op and returns 0.
 *  - On success, returns the total number of bytes read in [0, @p cap].
 *  - On error, returns a negative @ref fs_return_codes value; any bytes read
 *    before the error remain in @p buffer.
 *
 * @param file   Open file handle (must not be NULL).
 * @param buffer Destination buffer (must not be NULL when @p cap > 0).
 * @param cap    Total number of bytes to read.
 *
 * @return Bytes read (>= 0), 0 on EOF, or < 0 on error.
 */
ssize_t fs_read_all (struct fs_file *file, void *buffer, size_t cap);


/**
 * @brief Seek to absolute byte @p offset in an open file (if supported).
 *
 * @return @ref FS_OK on success, @ref FS_NOT_SUPPORTED if not available,
 *         or a negative error code.
 */
int fs_seek (struct fs_file *file, uint64_t offset);


/**
 * @brief Close an open file handle.
 *
 * @return @ref FS_OK on success or a negative error code.
 */
int fs_close(struct fs_file *file);

#endif /* FILESYSTEM_H */
