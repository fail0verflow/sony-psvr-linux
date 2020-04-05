#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>

static int console_getc(struct console *co)
{
	unsigned char c;
	co->read(co, &c, 1);
	return (int)c;
}

static void console_putc(struct console *co, char c)
{
	co->write(co, &c, 1);
}

static void console_puts(struct console *co, char *buf)
{
	int size = strlen(buf);
	co->write(co, buf, size);
}

static void console_move_cursor(struct console *co, char dir, int n)
{
	if (n) {
		console_putc(co, 0x1b);
		console_putc(co, '[');
		console_putc(co, n / 10 + '0');
		console_putc(co, n % 10 + '0');
		console_putc(co, dir);
	}
}

int console_read(unsigned char *buf, int count)
{
	struct console *co;
	int c, i, cursor = 0, size = 0;

	for (co = console_drivers; co; co = co->next) {
		if ((co->flags & CON_ENABLED) && co->read && co->write) {
			goto do_read;
		}
	}
	return '\0';

 do_read:
	while (1) {
		c = console_getc(co);

		if (c == 0x1b) { /* ESC */
			c = (c << 8) | console_getc(co);
		}

		switch (c) {
		case 0x06: /* ^F */
		case 0x1b43: /* Arrow Right */
			if (cursor >= size)
				continue;
			console_putc(co, buf[cursor++]);
			break;
		case 0x02: /*  ^B */
		case 0x1b44: /* Arrow Left */
			if (cursor == 0)
				continue;
			console_putc(co, 0x08);
			cursor--;
			break;
		case 0x1b34: /* DEL */
			if (cursor >= size)
				continue;
			console_putc(co, buf[cursor++]);
		case 0x08: /* BS */
			if (cursor == 0)
				continue;
			for (i = cursor; i < size; i++)
				buf[i-1] = buf[i];
			console_putc(co, 0x08);
			cursor--;
			size--;
			if (cursor == size) {
				console_putc(co, ' ');
				console_putc(co, 0x08);
			} else {
				buf[size] = '\0';
				console_puts(co, &buf[cursor]);
				console_putc(co, ' ');
				console_move_cursor(co, 'D', size-cursor+1);
			}
			break;
		case 0x0a: /* LF */
		case 0x0d: /* CR */
			buf[size] = '\0';
			console_puts(co, "\x0d\x0a");
			return size;
		case 0x03: /* ^C */
			console_puts(co, "^C\x0d\x0a");
			buf[0] = '\0';
			return 0;
		case 0x01: /* ^A */
			console_move_cursor(co, 'D', cursor);
			cursor = 0;
			break;
		case 0x05: /* ^E */
			console_move_cursor(co, 'C', size - cursor);
			cursor = size;
			break;
		default:
			if ((c < 0x20) || (c > 0x7e))
				continue;
			if (size > count)
				continue;
			for (i = size; i > cursor; i--)
				buf[i] = buf[i-1];
			buf[cursor] = c;
			size++;
			cursor++;
			console_putc(co, c);
			if (cursor != size)
				for (i = cursor; i < size; i++)
					console_putc(co, buf[i]);
			console_move_cursor(co, 'D', size - cursor);
		}
	}
}
EXPORT_SYMBOL(console_read);

void console_write(unsigned char *buf, int count)
{
	struct console *co;

	for (co = console_drivers; co; co = co->next) {
		if ((co->flags & CON_ENABLED) && co->write) {
			co->write(co, buf, count);
		}
	}
}
EXPORT_SYMBOL(console_write);

int flush_serial_tty(void)
{
	struct console *co;

	for (co = console_drivers; co; co = co->next) {
		if ((co->flags & CON_ENABLED)) {
#if 0
			if (co->flush_tty) {
				co->flush_tty(co);
			}
#else
#warning [TODO] needs to implement something to avoid a rare case.
			/* I'm not exactly sure it really happens or not, but if it does, some log 
			   messages shall be skipped to dump when an exception occurs. */
#endif
		}
	}


	return 0;
}
EXPORT_SYMBOL(flush_serial_tty);
