#pragma once
#include "global.h"

//These are implemented by the window manager; however, due to file operations being far more common than GUI, they're split off.

//Returns the working directory at the time of process launch.
//The true working directory is set to something unusable, and the program may not change or use it.
const char * window_get_cwd();

//Returns the process path, without the filename. Multiple calls will return the same pointer.
const char * window_get_proc_path();
//Converts a relative path (../roms/mario.smc) to an absolute path (/home/admin/roms/mario.smc).
// Implemented by the window manager, so gvfs can be supported. If the file doesn't exist, it is
// implementation defined whether the return value is a nonexistent path, or if it's NULL.
//basepath is the directory you want to use as base, or a file in this directory. NULL means current
// directory as set by window_cwd_enter, or that 'path' is expected absolute.
//If allow_up is false, NULL will be returned if 'path' attempts to go up the directory tree (for example ../../../../../etc/passwd).
//If path is absolute already, it will be returned (possibly canonicalized) if allow_up is true, or rejected otherwise.
//Send it to free() once it's done.
char * window_get_absolute_path(const char * basepath, const char * path, bool allow_up);
//Converts any file path to something accessible on the local file system. The resulting path can
// be both ugly and temporary, so only use it for file I/O, and store the absolute path instead.
//It is not guaranteed that window_get_absolute_path can return the original path, or anything useful at all, if given the output of this.
//It can return NULL, even for paths which file_read understands. If it doesn't, use free() when you're done.
char * window_get_native_path(const char * path);

class file {
private:
	file();//can't be used
public:
	class impl;
	friend class file::impl;
	
public:
	class impl : nocopy {
	protected:
		static file* wrap(impl* item) { return new file(item); }
		impl(size_t len) : len(len) {}
	public:
		const size_t len;
		virtual void read(size_t start, void* target, size_t len) = 0;
		virtual void* map(size_t start, size_t len) = 0;
		virtual void unmap(const void* data, size_t len) = 0;
		virtual ~impl(){}
		
		class mem;
	};
	class impl::mem : public impl {
		void* data;
	public:
		mem(void* data, size_t len) : impl(len), data(data) {}
		void read(size_t start, void* target, size_t len) { memcpy(target, (char*)data+start, len); }
		void* map(size_t start, size_t len) { return (char*)data+start; }
		void unmap(const void* data, size_t len) {}
		~mem() { free(data); }
	};
	
private:
	file::impl* core;
	//This one will memory map the file from the filesystem.
	//create() can simply return create_fs(filename), or can additionally support stuff like gvfs.
	static file::impl* create_fs(const char * filename);
	file(file::impl* core) : core(core), len(core->len) {}
public:
	size_t const len;
	
	class write;
	
	void read(size_t start, void* target, size_t len) { core->read(start, target, len); }
	void read(void* target, size_t len) { this->read(0, target, len); }
	
	//The mmap functions should be used only for large binary files, like ISOs. For anything else, use read().
	void* map_raw(size_t start, size_t len) { return core->mmap(start, len); }
	void* map_raw() { return this->map_raw(0, this->len); }
	void unmap(const void* data, size_t len) { core->unmap(data, len); }
	
	class map : autoref<map> {
		file::impl* parent;
	public:
		const void* const ptr;
		map(file::impl* parent, const void* ptr, size_t len) : parent(parent), ptr(ptr), len(len) {}
		void release() { this->parent->unmap(this->ptr, this->len); }
	private:
		size_t len;//gcc, shut up about initialization order, pointers and integers don't care
	};
	file::map mmap(size_t start, size_t len) { return file::map(core, this->map_raw(start, len), len); }
	file::map mmap() { return this->mmap(0, this->len); }
	
	
	operator bool() { return core; }
	
	~file() { delete core; }
	
	static file::impl* create_raw(const char * filename);
	static file create(const char * filename) { return file(create_raw(filename)); }
	static file::write create_update(const char * filename);
	static file::write create_overwrite(const char * filename);
};
//class file::mem : public file {
//public:
//	mem(void* data, size_t len) : file(data, len) {}
//	~mem() { free((void*)this->data); }
//};

class filewrite : nocopy {
private:
	static filewrite* create_fs(const char * filename, bool truncate);
protected:
	filewrite(){}
	
public:
	void* data;
	size_t len;
	
	static filewrite* create(const char * filename);
	static filewrite* create_replace(const char * filename);
	//If a file is grown, the new area has undefined values.
	virtual bool resize(size_t newsize) { return false; }
	//Sends all the data to the disk. Does not return until it's there.
	//The destructor also sends the data to disk, but does not guarantee that it's done immediately.
	//There may be more ways to send the file to disk, but this is not guaranteed either.
	virtual void sync(){}
};

//These are implemented by the window manager, despite looking somewhat unrelated.
//Support for absolute filenames is present.
//Support for relative filenames will be rejected as much as possible. However, ../../../../../etc/passwd may work.
//Other things, for example http://example.com/roms/snes/smw.sfc, may work too.
//Directory separator is '/', extension separator is '.'.
//file_read appends a '\0' to the output (whether the file is text or binary); this is not reported in the length.
//Use free() on the return value from file_read().
bool file_read(const char * filename, void* * data, size_t * len);
bool file_write(const char * filename, const anyptr data, size_t len);
bool file_read_to(const char * filename, anyptr data, size_t len);//If size differs, this one fails.

//Some simple wrappers for the above three.
inline bool file_read_rel(const char * basepath, bool allow_up, const char * filename, void* * data, size_t * len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_read(path, data, len);
	free(path);
	return ret;
}

inline bool file_write_rel(const char * basepath, bool allow_up, const char * filename, const anyptr data, size_t len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_write(path, data, len);
	free(path);
	return ret;
}

inline bool file_read_to_rel(const char * basepath, bool allow_up, const char * filename, anyptr data, size_t len)
{
	char* path=window_get_absolute_path(basepath, filename, allow_up);
	if (!path) return false;
	bool ret=file_read_to(path, data, len);
	free(path);
	return ret;
}

//These will list the contents of a directory. The returned paths from window_find_next should be
// sent to free(). The . and .. components will not be included; however, symlinks and other loops
// are not guarded against. It is implementation defined whether hidden files are included. The
// returned filenames are relative to the original path and contain no path information nor leading
// or trailing slashes.
void* file_find_create(const char * path);
bool file_find_next(void* find, char* * path, bool * isdir);
void file_find_close(void* find);

//If the window manager does not implement any non-native paths (like gvfs), it can use this one;
// it's implemented by something that knows the local file system, but not the window manager.
//There is no _window_native_get_native_path; since the local file system doesn't understand
// anything except the local file system, it would only be able to return the input, or be
// equivalent to _window_native_get_absolute_path, making it redundant and therefore useless.
char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up);
