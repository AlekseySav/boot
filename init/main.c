void main(void)
{	
    const char *naming = "does it finaly work?";
	char *video = (char*)0xb8000;
	unsigned int i = 0;
	unsigned int pix = 0;
	unsigned int displaysize;

	//screen_cleaning
	displaysize = 100 * 30 * 2;
	while (pix < displaysize) {
		video[pix] = ' ';
		video[pix+1] = 0x07;
		pix = pix + 2;
	}
	pix = 0;

	//screen_writing
	while (naming[pix] != '\0') {
		video[i] = naming[pix];
		video[i+1] = 0x07;
		++pix;
		i = i + 2;
	}
}

long stack[1024];

struct {
    long * ptr;
    short segment;
} __attribute__((__packed__)) stack_start = { &stack[1024], 0x10 };
