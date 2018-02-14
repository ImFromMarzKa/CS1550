/* 
 Riley Marzka
 ImFromMarzKa
 CS1550
 Project1
 2.5.17 
*/

/*Notes:
- Each library function must be implemented using Linux SYSCALLS ONLY
- NO C STANDARD LIBRARY FUNCTIONS ANYWHERE
*/

#include "library.h"
#include "iso_font.h"

int fid;
int fid1 = 1;
size_t size;
color_t *addr;

struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
struct termios term;

//Function to initialize graphics library
//Syscalls Used:
//	open
//	ioctl
//	mmap
void init_graphics(){

	//Open fb file descriptor
	fid = open("/dev/fb0", O_RDWR);
	if(fid == -1){
		perror("Error opening /dev/fb0");
		exit(1);
	}

	//Get screen size and bits per pixel
	if(ioctl(fid, FBIOGET_VSCREENINFO, &vinfo) == -1){
		perror("screen size");
		exit(1);
	}
	if(ioctl(fid, FBIOGET_FSCREENINFO, &finfo) == -1){
		perror("screen info");
		exit(1);
	}

	//Set size of mmap()'d file
	size = finfo.line_length * vinfo.yres_virtual;

	//Map file into our address space
	//addr is the pointer to the shared memory space with the frame buffer (fid)
	addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fid, 0);
	if(addr == (void *) -1){
		perror("Error mapping memory");
		exit(1);
	}

	//Disable keypress echo and buffering of keypresses
	if(ioctl(0, TCGETS, &term) == -1){
		perror("disable echo 1");
		exit(1);
	}
	term.c_lflag &= ~ECHO;
	term.c_lflag &= ~ICANON;
	if(ioctl(0, TCSETS, &term) == -1){
		perror("disable echo");
		exit(1);
	}

}

//Function to clear screen using ANSI escape code
//Syscalls Used:
//	write
void clear_screen(){
	write(fid1, "\033[2J", 4);
}

//Function to check is there is a keypress
//	if there is a keypress, then read it
//Syscalls Used:
//	select
//	read
char getkey(){
	//Create file descriptor and timeval
	fd_set rfds;
	struct timeval tv;
	int retval;

polling_loop:
	//Watch stdin (fd 0) to see when it has input
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	//Set time to wait to zero
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	retval = select(1, &rfds, NULL, NULL, &tv);
	//Error
	if(retval == -1){
		perror("select()");
	}
	//Key pressed
	else if(retval){
		//READ 
		char ch;
		retval = read(0, (void *)&ch, 1);
		//Error
		if(retval == -1){
			perror("read()");
			exit_graphics();
		}
		else{
			return ch;
		}
	}
	//No key Pressed
	else{
		//Try again
		goto polling_loop;
	}
}

//Function to make program sleep between frames of graphics being drawn
//Syscalls Used:
//	nanosleep
void sleep_ms(long ms){
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = ms * 1000000;
	int ret = nanosleep(&t, NULL);
	if(ret == -1){
		perror("nanosleep()");
	}
}

//Function to serve as main drawing code
//Syscalls Used:
//	NONE
void draw_pixel(int x, int y, color_t color){
	//Create offset pointer
	color_t off = 0;
	//Set pointer value
	off = (color_t)(x + y*vinfo.xres_virtual);
	//Traverse to pixel to set
	color_t *pixel = addr + off;
	//Set pixel to color
	*pixel = RMASK(color) | GMASK(color) | BMASK(color);
}

//Function to make a rectangle using draw_pixel()
//Syscalls Used:
//	NONE
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i, j;
	//Draw line from (x1, y1) to (x1+width, y1)
	for(i = x1; i <= (x1+width); i++){
		draw_pixel(i, y1, c);
	}
	//Draw line from (x1+width, y1) to (x1+width, y1+height)
	for(j = y1; j <= (y1+height); j++){
		draw_pixel(i, j, c);
	}
	//Draw line from (x1+width, y1+height) to (x1, y1+height)
	for(i = x1+width; i >= x1; i--){
		draw_pixel(i, j, c);
	}
	//Draw line from (x1, y1+height) to (x1, y1)
	for(j = y1+height; j >= y1; j--){
		draw_pixel(i, j, c);
	}
}

//Function to draw string with specified color at starting location (x, y)
//Syscalls Used:
//	NONE
void draw_text(int x, int y, const char *text, color_t c){
	//Iterate through string until null term
	while(*text != '\0'){
		//Draw one character
		draw_char(x, y, *text, c);
		//Increment x for next character, 
		//allowing for one column of pixels
		//between each char
		x += 9;
		//Traverse to next character in string
		text += 1;
	}
}

//Function to draw a single character
//top left of character is at coordinate (x, y)
void draw_char(int x, int y, char ch, color_t c){
	//Set temp values for x and y
	int _x = x;
	int _y = y;
	//Get starting index
	int index = ch * 16;
	int i, rowVal;
	//Traverse through the subsequent indices
	//iso_font[index+0] through iso_font[index+15]
	for(i = 0; i < 16; i++){
		//Get the 8-bit integer value contained at each index
		rowVal = iso_font[index+i];
		int j, temp;
		short bit;

		//Traverse each bit of the integer
		for(j = 0; j < 8; j++){
			//Shift the integer left 
			temp = rowVal >> j;
			//Store MSB in 'bit'
			bit = temp & 1;

			//Check bit
			if(bit){
				//if 1, then draw a pixel
				draw_pixel(_x, _y, c);
			}
			//Increment _x to move to next pixel
			_x++;
		}
		//Reset x coordinate and move to next y
		_x = x;
		_y++;
	}
}

//Function to clean up before the program exits
//Syscalls Used:
//	ioctl
void exit_graphics(){
	clear_screen();
	
	//Reset terminal settings
	if(ioctl(0, TCGETS, &term) == -1){
		perror("reset term settings 1");
		exit(1);
	}
	term.c_lflag |= ECHO;
	term.c_lflag |= ICANON;
	if(ioctl(0, TCSETS, &term) == -1){
		perror("reset term settings 2");
		exit(1);
	}

	//Unmap memory
	if(munmap(addr, size) == -1){
		perror("Error unmapping memory");
		exit(1);
	}
	//close open files
	if(!close(fid)){

		exit(0);
	}
	else{
		perror("Error closing /dev/fb0");
		exit(1);
	}
}