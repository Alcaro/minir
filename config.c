#include "minir.h"

struct minirconfig_impl {
	struct minirconfig i;
	
};

static const char * get_autoload(struct minirconfig * this)
{
	return NULL;
}

static const char * * get_supported_extensions(struct minirconfig * this)
{
	return NULL;
}

static struct configcorelist * get_core_for(struct minirconfig * this, const char * gamepath, unsigned int * count)
{
	return NULL;
}

static void data_load(struct minirconfig * this, struct configdata * config,
                      bool free_old, const char * corepath, const char * gamepath)
{
	
}

static void data_save(struct minirconfig * this, struct configdata * config)
{
	
}

static void data_free(struct minirconfig * this, struct configdata * config)
{
	
}

static void data_destroy(struct minirconfig * this, const char * item)
{
	
}

static void write(struct minirconfig * this, const char * path)
{
	
}

static void free_(struct minirconfig * this)
{
	
}

struct minirconfig * config_create(const char * path)
{
	struct minirconfig_impl * this=malloc(sizeof(struct minirconfig_impl));
	this->i.get_autoload=get_autoload;
	this->i.get_supported_extensions=get_supported_extensions;
	this->i.get_core_for=get_core_for;
	this->i.data_load=data_load;
	this->i.data_save=data_save;
	this->i.data_free=data_free;
	this->i.data_destroy=data_destroy;
	this->i.write=write;
	this->i.free=free_;
	
	return (struct minirconfig*)this;
}
