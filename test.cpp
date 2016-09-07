#include "RetroArch/gfx/video_driver.h"
#undef ARRAY_SIZE
#include "arlib.h"

//video_xshm.foo;

uint32_t pixels[320*240];

video_driver_t* driver;
void* driver_data;

int main(int argc, char* argv[])
{
	window_init(&argc, &argv);
	
	widget_viewport* port = widget_create_viewport(1024, 768);
	window* wndw = window_create(port);
	wndw->set_menu(windowmenu_menu::create_top(windowmenu_menu::create("foo", windowmenu_item::create("bar", NULL), NULL), NULL));
	wndw->set_visible(true);
	
	video_info_t info = { 1024, 768, false, true, false, false, 1, true, port->get_window_handle() };
	
	driver = &video_xshm;
	driver_data = driver->init(&info, NULL, NULL);
	
	int i = 0;
	while (wndw->is_visible())
	{
		memset(pixels, i++, sizeof(pixels));
		driver->frame(driver_data, pixels, 320, 240, 0, sizeof(uint32_t)*320, "");
		window_run_iter();
	}
}
