#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "constants/characters.h"
#include "native_text_input.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
int main(void)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO));
    NativeTextBegin(); CHECK(NativeTextActive());
    NativeTextUtf8("Ab 9\xc3\xa9!\xf0\x9f\x90\x89");
    CHECK(NativeTextPop() == CHAR_A && NativeTextPop() == CHAR_b);
    CHECK(NativeTextPop() == CHAR_SPACE && NativeTextPop() == CHAR_9);
    CHECK(NativeTextPop() == CHAR_e_ACUTE && NativeTextPop() == CHAR_EXCL_MARK && NativeTextPop() == -1);
    NativeTextUtf8("\xe2\x99\x82\xe2\x99\x80\xe2\x80\xa6");
    CHECK(NativeTextPop() == CHAR_MALE && NativeTextPop() == CHAR_FEMALE && NativeTextPop() == CHAR_ELLIPSIS);
    CHECK(NativeTextKey(SDL_SCANCODE_BACKSPACE, 1) && NativeTextPop() == NATIVE_TEXT_BACKSPACE);
    CHECK(NativeTextKey(SDL_SCANCODE_RETURN, 0) && NativeTextPop() == NATIVE_TEXT_ACCEPT);
    CHECK(!NativeTextKey(SDL_SCANCODE_RETURN, 1));
    NativeTextUtf8("x"); NativeTextEnd(); CHECK(!NativeTextActive() && NativeTextPop() == -1);
    SDL_Quit(); puts("native text input, accents, deletion and confirmation passed"); return 0;
}
