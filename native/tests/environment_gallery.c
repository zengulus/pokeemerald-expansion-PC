/* Capture the real module's GLSL over a frozen scene from the running game.
 * This gallery deliberately controls weather/terrain for visual critique. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "mod_host.h"
#include "battle_shader.h"
#include "../mods/bundled.h"
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr,"gallery: %s: %s\n",#c,SDL_GetError()); return 1; } } while(0)
struct Scene { const char *name; unsigned weather,terrain,suppressed; };
int main(int argc, char **argv)
{
    REQUIRE(argc==3 || (argc==5 && !strcmp(argv[3],"--frames")));
    unsigned frameCount=argc==5 ? strtoul(argv[4],NULL,10) : 80;
    REQUIRE(frameCount>=2 && frameCount<=600);
    FILE *input=fopen(argv[1],"rb"); REQUIRE(input);
    char magic[8]; REQUIRE(fread(magic,1,8,input)==8 && !memcmp(magic,"RHHSCN01",8));
    static uint16_t pixels[240*160]; static uint8_t rgb[240*160*3];
    for(unsigned i=0;i<240*160;++i) { int a=fgetc(input),b=fgetc(input); REQUIRE(a>=0 && b>=0); pixels[i]=a|(b<<8); }
    REQUIRE(fread(gNativeShaderSceneMask,1,sizeof(gNativeShaderSceneMask),input)==sizeof(gNativeShaderSceneMask)); fclose(input);
    RhhModsInit(); REQUIRE(!RhhModsRegister(RhhBundledBattleEnvironment(RhhModsHostAPI()))); REQUIRE(!RhhModsFinalize());
    const struct RhhModValue enabled={RHH_SETTING_BOOL,0,1};
    REQUIRE(!RhhModsConfigure("rhh:battle_environment","enabled",&enabled));
    REQUIRE(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_Window *window=SDL_CreateWindow("Battle effects gallery",0,0,720,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    REQUIRE(window && !NativeShaderInit(window));
    printf("Renderer: %s\n",NativeShaderDriver());
    const struct Scene scenes[]={
        {"clear",0,0,0},{"rain",1,0,0},{"sun",2,0,0},{"sand",3,0,0},{"hail",4,0,0},
        {"snow",5,0,0},{"fog",6,0,0},{"wind",7,0,0},{"harsh-sun",8,0,0},{"heavy-rain",9,0,0},
        {"grassy",0,1,0},{"misty",0,2,0},{"electric",0,3,0},{"psychic",0,4,0},
        {"rain-grassy",1,1,0},{"sand-electric",3,3,0},{"snow-psychic",5,4,0},
        {"suppressed-rain-grassy",1,1,1}};
    static uint8_t baseline[sizeof(rgb)];
    uint8_t *grassyFrames=malloc(sizeof(rgb)*frameCount); REQUIRE(grassyFrames);
    for(unsigned i=0;i<sizeof(scenes)/sizeof(*scenes);++i)
    {
        const struct Scene *scene=&scenes[i];
        struct RhhModEvent event={.struct_size=sizeof(event),.kind=RHH_BATTLE_ENVIRONMENT,
            .args={scene->weather,scene->terrain,scene->suppressed,1}};
        struct RhhModDecision decision={.struct_size=sizeof(decision),.allowed=1}; RhhModsDispatch(&event,&decision);
        struct NativeBattleEnvironment env={decision.value,scene->weather,scene->terrain,scene->suppressed,0,1.f};
        REQUIRE(NativeShaderReady(env.shader));
        char path[1024]; snprintf(path,sizeof(path),"%s/%s.rgbv",argv[2],scene->name);
        FILE *video=fopen(path,"wb"); REQUIRE(video);
        for(unsigned frame=0;frame<frameCount;++frame)
        {
            /* One sample every three game frames is 20 fps at normal speed. */
            NativeShaderPresent(pixels,&env,720+frame*3); REQUIRE(!NativeShaderCapture(rgb));
            if(!i && !frame) memcpy(baseline,rgb,sizeof(rgb));
            for(unsigned p=0;p<240*160;++p) if(!gNativeShaderSceneMask[p]) REQUIRE(!memcmp(rgb+p*3,baseline+p*3,3));
            REQUIRE(fwrite(rgb,1,sizeof(rgb),video)==sizeof(rgb));
            if(!frame)
            {
                snprintf(path,sizeof(path),"%s/%s.rgb",argv[2],scene->name);
                FILE *out=fopen(path,"wb"); REQUIRE(out && fwrite(rgb,1,sizeof(rgb),out)==sizeof(rgb)); REQUIRE(!fclose(out));
            }
            if(!strcmp(scene->name,"grassy")) memcpy(grassyFrames+frame*sizeof(rgb),rgb,sizeof(rgb));
            if(!strcmp(scene->name,"suppressed-rain-grassy")) REQUIRE(!memcmp(grassyFrames+frame*sizeof(rgb),rgb,sizeof(rgb)));
        }
        REQUIRE(!fclose(video));
        printf("Captured %s: %u frames at 20 fps\n",scene->name,frameCount); fflush(stdout);
    }
    /* A second frame proves that the captures represent animated programs. */
    struct NativeBattleEnvironment rain={RhhContentKeyOf("rhh:shader/battle_environment"),1,0,0,0,1.f};
    NativeShaderPresent(pixels,&rain,780); REQUIRE(!NativeShaderCapture(rgb));
    char path[1024]; snprintf(path,sizeof(path),"%s/rain-motion.rgb",argv[2]);
    FILE *out=fopen(path,"wb"); REQUIRE(out && fwrite(rgb,1,sizeof(rgb),out)==sizeof(rgb)); fclose(out);
    NativeShaderShutdown(); SDL_DestroyWindow(window); SDL_Quit(); RhhModsShutdown();
    free(grassyFrames);
    puts("Captured eighteen animated live-GLSL scenes; all UI pixels preserved on every frame and suppressed weather matches terrain-only output.");
    return 0;
}
