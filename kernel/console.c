// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define SPACE 0x95
#define CRTPORT 0x3d4

#define belaCrna 0x0700
#define ljubBela 0x7500
#define crvVoda 0x1600
#define belaZuta 0x6700

static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory
static ushort crtTemp[25*80];
extern int altPressed;

char matrix[9][9] = {
        
        "_________",
        "|WHT BLK|",
        "_________",
        "|PUR WHT|",
        "_________",
        "|RED AQU|",
        "_________",
        "|WHT YEL|",
        "_________"
    
    };

static char flag = 0; //tabelaMoud
static int marker = 0;
static int kurentPeint = 0x0700;

static void
cgaputc(int c)
{
	int pos;
	 
	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(flag == 0){ // flag
		if(c == '\n')
			pos += 80 - pos%80;
		else if(c == BACKSPACE){
			if(pos > 0) --pos;
		} else if(c == 't'){
			crt[pos++] = (c&0xff) | kurentPeint;
		} else
			crt[pos++] = (c&0xff) | kurentPeint;			
			//crt[pos++] = (c&0xff) | 0x0700;  // black on white

		if(pos < 0 || pos > 25*80)
			panic("pos under/overflow");

		if((pos/80) >= 24){  // Scroll up.
			memmove(crt, crt+80, sizeof(crt[0])*23*80);
			pos -= 80;
			memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
		}

		outb(CRTPORT, 14);
		outb(CRTPORT+1, pos>>8);
		outb(CRTPORT, 15);
		outb(CRTPORT+1, pos);
		crt[pos] = ' ' | kurentPeint;
	} else {
		int currX = pos%80+1;
		int currY = pos/80;

		if(currX > 70){
			currX -= 9;
		}
		if(currY >= 10){
			currY -= 9;
		}
		
		// granice tabele
		int tableStartX = currX;
		int tableStartY = currY;
		int tableEndX = tableStartX + 9;
		int tableEndY = tableStartY + 9;
		
		for(int i = 0; i < 9; i++){
			for(int j = 0; j < 9; j++){
				int tableCurrX = currX + j;
				int tableCurrY = currY + i;
				
				if(marker == 0 && i == 1 && (j != 0 || j != 9)){
					crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x2000;   // matrix[i][j] je karakter koji se stampa
					kurentPeint = belaCrna;
				} else if (marker == 1 && i == 3 && (j != 0 || j != 9)) {
					crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x2000;
					kurentPeint = ljubBela;
				} else if (marker == 2 && i == 5 && (j != 0 || j != 9)) {
					crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x2000;
					kurentPeint = crvVoda;
				} else if (marker == 3 && i == 7 && (j != 0 || j != 9)) {
					crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x2000;
					kurentPeint = belaZuta;
				} else {
					crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x7000;
				}
				
				//crt[tableCurrX + 80*tableCurrY] = (matrix[i][j]&0xff) | 0x7000;
				

			}
		}
	
		for(int y = 0; y < 25; y++){
			for(int x = 0; x < 80; x++){
				if(x >= tableStartX && x < tableEndX && y >= tableStartY && y < tableEndY)
					continue;
				
				char ch = crt[x+80*y] & 0xff;
				crt[x+80*y] = ch | kurentPeint;
			}
		}

	}
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case 't':
			if(altPressed == 1){ 
				if(flag == 0){
					memcpy(crtTemp, crt, sizeof(ushort)*80*25);
					flag = 1;
					consputc(c);
				} else {
					memcpy(crt, crtTemp, sizeof(ushort)*80*25);
					
					//  repaintujemo skrin kad gasimo tabelu
					for(int y = 0; y < 25; y++){
						for(int x = 0; x < 80; x++){
							char ch = crt[x+80*y] & 0xff;
							crt[x+80*y] = ch | kurentPeint;
						}
					}

					flag = 0;
					//consputc(c);
				}
				altPressed = 0;
			} else if(flag == 0 && altPressed == 0){
				//consputc(c);
				if(c != 0 && input.e-input.r < INPUT_BUF){
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
			}
			break;
		case 'w':
			if(flag == 1){
				marker++;
				if(marker > 3){
					marker = 0;
				}
			} else {
				if(c != 0 && input.e-input.r < INPUT_BUF){
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					//consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
				
			}
			consputc(c);
			break;
		case 's':
			if(flag == 1){
				marker--;
				if(marker < 0){
					marker = 3;
				}
			} else {
				if(c != 0 && input.e-input.r < INPUT_BUF){
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					//consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
			}
			consputc(c);
			break;
		case '\r': case '\n':
			if(flag == 1){
				memcpy(crt, crtTemp, sizeof(ushort)*80*25);
				
				//  repaintujemo skrin kad gasimo tabelu
				for(int y = 0; y < 25; y++){
					for(int x = 0; x < 80; x++){
						char ch = crt[x+80*y] & 0xff;
						crt[x+80*y] = ch | kurentPeint;
					}
				}

				flag = 0;
				break;
			} else {
				if(c != 0 && input.e-input.r < INPUT_BUF){
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				if(flag == 0){ // da se bafer ne puni djubretom kad smo u tabela modu
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

