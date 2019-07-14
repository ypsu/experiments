#ifndef menu_h
#define menu_h

enum { MENU_ENTRIES = 14 };
enum { MENU_NAMELEN = 16 };
enum { MENU_MAXOPTS = 10 };

struct menu_option {
	int offset, length;
	char name[MENU_NAMELEN+1];
};

struct menu_entry {
	bool active;
	char name[MENU_NAMELEN+1];
	char hint[MAXSTR];
	int options_count;
	int selected_option;
	struct menu_option options[MENU_MAXOPTS];
};

struct menu {
	struct menu_entry entries[MENU_ENTRIES];
};

void menu_init(void);
void menu_destroy(void);
void menu_render(void);
void menu_click(const vec_t *pos);
void menu_show(void);

#endif
