#ifndef RHH_NATIVE_TEXT_INPUT_H
#define RHH_NATIVE_TEXT_INPUT_H
enum { NATIVE_TEXT_BACKSPACE = 256, NATIVE_TEXT_ACCEPT = 257, NATIVE_TEXT_CANCEL = 258 };
void NativeTextBegin(void);
int NativeTextBeginAscii(void);
void NativeTextEnd(void);
int NativeTextActive(void);
int NativeTextPop(void);
void NativeTextUtf8(const char *text);
int NativeTextKey(int scancode, int repeat);
#endif
