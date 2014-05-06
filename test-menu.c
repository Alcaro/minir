#ifdef TEST
#include "minir.h"
#include<stdio.h>

struct windowmenu * m;
struct windowmenu * r;
struct windowmenu * d;
void q(struct windowmenu*a,void*s)
{
	puts("q");
	d->set_state(d,!d->get_state(d));
	m->insert_child(m, windowmenu_create_item("b", NULL, NULL), 0);
}

void q2(struct windowmenu*f,bool q,void*d)
{
	printf("%i",q);
	puts("q");
	r->set_enabled(r,q);
	//m->remove_child(m, r);
}

void q3(struct windowmenu*f,unsigned int q,void*d)
{
	printf("%i",q);
	puts("q");
}

int main(int argc, char * argv[])
{
	window_init(&argc, &argv);
	
	struct window * wndw=window_create(widget_create_viewport(200, 200));
	wndw->set_visible(wndw, true);
	
	wndw->set_menu(wndw,
		windowmenu_create_topmenu(
			m=windowmenu_create_submenu("__File",
				r=windowmenu_create_radio(q3, NULL, "a", "b", "c", NULL),
				windowmenu_create_item("q", q, NULL),
				d=windowmenu_create_check("q2", q2, NULL),
				NULL),
			NULL)
		);
	
	while (wndw->is_visible(wndw)) window_run_iter();
}
#endif
