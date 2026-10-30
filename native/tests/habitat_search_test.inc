static void HabitatSearchMode(u32 mode)
{
    struct RhhModEvent event = {.struct_size = sizeof(event), .kind = RHH_MOD_UPDATE, .args = {mode}};
    struct RhhModDecision decision = {.struct_size = sizeof(decision), .allowed = 1}; RhhModsDispatch(&event, &decision);
}
