/* 
 Riley Marzka
 ImFromMarzKa
 CS1550
 Project1
 2.5.17 
*/

/*Notes:
 - Driver for library.c to show functionality
*/

#include "library.h"

void testKeys();

int x = (640-20)/2;
int y = (480-20)/2;

int main(int argc, char** argv){

	init_graphics();

	//Draw a 20x20 blue rectangle 
	draw_rect(x, y, 20, 20, 500);
	sleep_ms(500);

	//Draw a string
	draw_text(x, y+25, "This is a rectangle!", 5000);
	sleep_ms(500);
	clear_screen();

	//Test keys
	testKeys();
	sleep_ms(500);

	exit_graphics();

	return 0;
}

void testKeys(){
	const char *a = "a = Hello";
	const char *b = "b = World";
	const char *c = "c = Riley";
	const char *q = "q to quit";

	int _y = y;

	draw_text(x, _y, a, 20);
	_y += 20;
	draw_text(x, _y, b, 20);
	_y += 20;
	draw_text(x, _y, c, 20);
	_y += 20;
	draw_text(x, _y, q, 20);

	char in;
	int i;
	for(i = 0; i < 4; i++){
		in = getkey();
		if(i == 0){
			_y = y;
			clear_screen();
		}
		if(in == 'a'){
			draw_text(x, _y, (a + 4), 25000);
		}
		else if(in == 'b'){
			draw_text(x, _y, (b + 4), 32769);
		}
		else if(in == 'c'){
			draw_text(x, _y, (c + 4), 46525);
		}
		else if(in == 'q'){
			exit_graphics();
		}
		else{
			draw_text(x, _y, "Invalid", 20);
		}
		_y += 20;
	}
}
