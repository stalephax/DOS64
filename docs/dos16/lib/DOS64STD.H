/* DOS64STD.H - pont standard minimal pour programmes MS-DOS 16-bit */

#ifndef DOS64STD_H
#define DOS64STD_H

#ifdef __cplusplus
extern "C" {
#endif

void dos64_putc(char ch);
void dos64_puts(const char* dollar_terminated_string);
unsigned short dos64_get_version(void);
void dos64_exit(unsigned char code);

#ifdef __cplusplus
}
#endif

#endif
