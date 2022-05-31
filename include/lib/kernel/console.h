#ifndef __LIB_KERNEL_CONSOLE_H
#define __LIB_KERNEL_CONSOLE_H

void console_init (void);
void console_panic (void);
void console_print_stats (void);

int vprintf (const char *format, va_list args);
int puts (const char *s);
// 
void putbuf (const char *buffer, size_t n);
int putchar (int c);
static void vprintf_helper (char c, void *char_cnt_);
static void putchar_have_lock (uint8_t c);
#endif /* lib/kernel/console.h */
