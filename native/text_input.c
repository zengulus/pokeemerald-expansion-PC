#include <SDL2/SDL.h>
#include "constants/characters.h"
#include "native_text_input.h"

static int sActive, sAscii, sQueue[128];
static unsigned sHead, sTail;
static void Push(int value)
{
    unsigned next = (sTail + 1) % 128;
    if (next != sHead) { sQueue[sTail] = value; sTail = next; }
}
void NativeTextBegin(void)
{ sHead = sTail = 0; sActive = 1; sAscii = 0; SDL_StartTextInput(); }
int NativeTextBeginAscii(void)
{ if (sActive) return -1; sHead = sTail = 0; sActive = sAscii = 1; SDL_StartTextInput(); return 0; }
void NativeTextEnd(void)
{ sActive = sAscii = 0; sHead = sTail = 0; SDL_StopTextInput(); }
int NativeTextActive(void) { return sActive; }
int NativeTextPop(void)
{
    if (sHead == sTail) return -1;
    int value = sQueue[sHead]; sHead = (sHead + 1) % 128; return value;
}
int NativeTextKey(int code, int repeat)
{
    if (!sActive) return 0;
    if (sAscii && code == SDL_SCANCODE_ESCAPE && !repeat) { Push(NATIVE_TEXT_CANCEL); return 1; }
    if (code == SDL_SCANCODE_BACKSPACE) { Push(NATIVE_TEXT_BACKSPACE); return 1; }
    if ((code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_KP_ENTER) && !repeat)
    { Push(NATIVE_TEXT_ACCEPT); return 1; }
    return 0;
}
void NativeTextUtf8(const char *text)
{
    if (!sActive || !text) return;
    if (sAscii)
    {
        for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
            if (*p >= 32 && *p <= 126) Push(*p);
        return;
    }
    const unsigned char *p = (const unsigned char *)text;
    while (*p)
    {
        unsigned c = *p++;
        if (c >= 0xc2 && c <= 0xdf && (*p & 0xc0) == 0x80) c = ((c & 31) << 6) | (*p++ & 63);
        else if (c >= 0xe0 && c <= 0xef && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80)
        { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
        else if (c >= 128) continue;
        int out = -1;
        if (c >= 'A' && c <= 'Z') out = CHAR_A + c - 'A';
        else if (c >= 'a' && c <= 'z') out = CHAR_a + c - 'a';
        else if (c >= '0' && c <= '9') out = CHAR_0 + c - '0';
        else switch (c)
        {
        case ' ': out = CHAR_SPACE; break; case '.': out = CHAR_PERIOD; break;
        case ',': out = CHAR_COMMA; break; case '-': out = CHAR_HYPHEN; break;
        case '!': out = CHAR_EXCL_MARK; break; case '?': out = CHAR_QUESTION_MARK; break;
        case '/': out = CHAR_SLASH; break; case '\'': out = CHAR_SGL_QUOTE_RIGHT; break;
        case 0xe9: out = CHAR_e_ACUTE; break; case 0xc9: out = CHAR_E_ACUTE; break;
        case 0xe8: out = CHAR_e_GRAVE; break; case 0xe4: out = CHAR_a_DIAERESIS; break;
        case 0xf6: out = CHAR_o_DIAERESIS; break; case 0xfc: out = CHAR_u_DIAERESIS; break;
        case 0xf1: out = CHAR_n_TILDE; break; case 0xe7: out = CHAR_c_CEDILLA; break;
        case 0xc4: out = CHAR_A_DIAERESIS; break; case 0xd6: out = CHAR_O_DIAERESIS; break;
        case 0xdc: out = CHAR_U_DIAERESIS; break;
        case 0x2642: out = CHAR_MALE; break; case 0x2640: out = CHAR_FEMALE; break;
        case 0x2026: out = CHAR_ELLIPSIS; break;
        }
        if (out >= 0) Push(out);
    }
}
