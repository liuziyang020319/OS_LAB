#include <bios.h>

int getchar()
{
	int ch = -1;
	while (ch == -1) {
		ch = bios_getchar();
	}
	return ch;
}

int main(void)
{
	int ch;
	while ((ch = getchar()) != '\n' && ch != '\r'){
		if(ch>0)bios_putchar(ch);
	};
	bios_putstr("test1 APP\n");
	return 0;
}