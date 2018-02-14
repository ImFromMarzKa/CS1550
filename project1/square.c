#include <stdio.h>

typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_s(unsigned seconds);


int main(int argc, char** argv)
{
	int i;

	init_graphics();

	//char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	for(i = 0; i < 4; i++)
	{
		//draw a black rectangle to erase the old one
		draw_line(0);
		draw_line(0);
		draw_line(0);
		draw_line(0);

		//draw a blue rectangle
		draw_line(15);
		draw_line(15);
		draw_line(15);
		draw_line(15);
		
		sleep_s(2);
	}

	exit_graphics();

	return 0;

}
