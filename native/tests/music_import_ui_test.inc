#include "native_text_input.h"
static void CheckWindowsFileDialog(const char *module)
{
    CHECK(!SDL_Init(SDL_INIT_VIDEO)); InitModsForTestWithLibrary(module); gModDraft = (struct ModConfig){0}; ModsNewGame(99);
    const struct RhhModHostV1 *host = RhhModsHostAPI();
    const struct RhhFileDialogService *files = (const void *)host->get_service("rhh:service/file_dialog", RHH_SERVICE_VERSION(1, 0));
    const struct RhhUiService *ui = (const void *)host->get_service("rhh:service/ui", RHH_SERVICE_VERSION(1, 0));
    CHECK(files && ui);
    RhhContentKey owner = RhhContentKeyOf("test:music_preview_probe");
    struct RhhModEvent tick = {.struct_size = sizeof(tick), .kind = RHH_MOD_UPDATE};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1};
    RhhModsDispatch(&tick, &decision); CHECK(ui->active());
    CHECK(files->begin(owner, RHH_FILE_DIALOG_MIDI) == RHH_FILE_DIALOG_OK);
    SDL_Delay(1000);
    struct RhhFileDialogResult file = {.struct_size = sizeof(file)};
    CHECK(files->poll(owner, &file) == RHH_FILE_DIALOG_OK && file.state == RHH_FILE_DIALOG_PENDING);
    CHECK(files->cancel(owner) == RHH_FILE_DIALOG_OK);
    for (u32 i = 0; i < 2000; ++i)
    {
        SDL_Delay(5); CHECK(files->poll(owner, &file) == RHH_FILE_DIALOG_OK);
        if (file.state != RHH_FILE_DIALOG_PENDING) break;
    }
    CHECK(file.state == RHH_FILE_DIALOG_CANCELLED && !file.path[0] && files->take(owner, &file) == RHH_FILE_DIALOG_OK);
    CHECK(files->begin(owner, RHH_FILE_DIALOG_SOUNDFONT) == RHH_FILE_DIALOG_OK);
    SDL_Delay(500); CHECK(!ui->close(owner));
    RhhFileDialogShutdown(); RhhModsUnloadLibraries(); SDL_Quit();
    puts("windows picker: native dialog and panel cancellation passed");
}
