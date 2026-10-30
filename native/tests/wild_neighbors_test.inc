static void CheckWildQueueMissingOwner(const char *path, const char *identities)
{
    InitModsForTest(); CHECK(!NativeSaveOpen(path)); gFlashMemoryPresent = TRUE;
    CHECK(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    struct RhhWildActorIndividual ids[6]; FILE *idFile = fopen(identities, "rb");
    CHECK(idFile && fread(ids, sizeof(ids), 1, idFile) == 1 && !fclose(idFile));
    const struct RhhWildActorService *actors = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_actor", RHH_SERVICE_VERSION(1, 5));
    const struct RhhWildBehaviorService *behavior = (const void *)RhhModsHostAPI()->get_service("rhh:service/wild_behavior", RHH_SERVICE_VERSION(1, 4));
    struct RhhWildActorSnapshot actor = {.struct_size = sizeof(actor)};
    struct RhhWildQueue queue = {.struct_size = sizeof(queue)};
    CHECK(actors && behavior && !actors->find_individual(&ids[0], &actor) && !behavior->queue_at(actor.handle, &queue)
        && queue.count == 2 && queue.revision == 1 && !queue.owner_available
        && queue.owner == RhhContentKeyOf("test:wild_neighbors"));
    struct RhhWildBehaviorInspectionRequest inspect = {.struct_size = sizeof(inspect), .actor_handle = actor.handle,
        .map = actor.map, .x = actor.x, .y = actor.y};
    struct RhhWildBehaviorInspection inspection = {.struct_size = sizeof(inspection)};
    CHECK(!behavior->inspect(&inspect, &inspection) && (inspection.gates & RHH_WILD_GATE_NO_HOOKS));
    CHECK(TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
    NativeSaveClose(); RhhModsUnloadLibraries(); puts("wild queue: missing owner retained across save");
}
