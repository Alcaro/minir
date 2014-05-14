#include "minir.h"

static void set_memory(struct minircheats_model * this_, const struct libretro_memory_descriptor * memory, unsigned int nummemory) {}
static void search_reset(struct minircheats_model * this_) {}
static void search_set_datsize(struct minircheats_model * this_, unsigned int datsize) {}
static void search_set_signed(struct minircheats_model * this_, bool issigned) {}
static void search_do_search(struct minircheats_model * this_, enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto) {}
static unsigned int search_get_num_rows(struct minircheats_model * this_) { return 0; }
static void search_get_vis_row(struct minircheats_model * this_, unsigned int row, char * addr, uint32_t * val, uint32_t * prevval) {}
static void free_(struct minircheats_model * this_) {}

struct minircheats_model minircheats_model_dummy = {
	set_memory,
	search_reset, search_set_datsize, search_set_signed, search_do_search, search_get_num_rows, search_get_vis_row,
	NULL, NULL, NULL,//cheat_read, cheat_find_for_addr, cheat_get_count,
	NULL, NULL, NULL, NULL,//cheat_set, cheat_set_as_code, cheat_get, cheat_get_as_code,
	NULL, NULL, NULL,//cheat_set_enabled, cheat_remove, cheat_apply,
	free_
};
struct minircheats_model * minircheats_create_model()
{
	return &minircheats_model_dummy;
}
