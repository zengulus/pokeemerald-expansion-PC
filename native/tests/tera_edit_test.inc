static const struct RhhMonEditService *sTeraEditor;
static const struct RhhTeraEditRequest *sTeraRequest;
static u32 sTeraEditMode, sTeraEditProposals, sTeraEditReceipts;
static void TeraEditConstraint(const struct RhhModEvent *event, const void *state, struct RhhModDecision *decision)
{
    (void)state;
    if (!(event->args[0] & RHH_EDIT_TERA)) return;
    struct RhhTeraEditContext context = {.struct_size = sizeof(context)};
    struct RhhTeraEditResult nested = {.struct_size = sizeof(nested)};
    CHECK(!sTeraEditor->tera_context(&context) && context.is_preview == event->args[1]
        && context.before.stored_ordinal != context.after.stored_ordinal
        && sTeraEditor->preview_tera(sTeraRequest, &nested) == RHH_EDIT_UNAVAILABLE);
    ++sTeraEditProposals;
    if (sTeraEditMode == 1) { decision->allowed = 0; snprintf(decision->reason, sizeof(decision->reason), "Tera denied"); }
    if (sTeraEditMode == 2) { (void)Random(); (void)Random2(); }
    if (sTeraEditMode == 4) gSaveBlock1Ptr->daycare.mons[0].mon = gParties[B_TRAINER_PLAYER][0].box;
}
static void TeraEditReceipt(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    if (!(event->args[0] & RHH_EDIT_TERA)) return;
    struct RhhTeraEditContext context = {.struct_size = sizeof(context)};
    CHECK(!sTeraEditor->tera_context(&context) && !context.is_preview);
    ++sTeraEditReceipts;
    if (sTeraEditMode == 3) { (void)Random(); (void)Random2(); }
}
static const struct RhhModHook sTeraEditHooks[] = {
    {sizeof(struct RhhModHook), RHH_CAN_EDIT_MON, RHH_CONSTRAINT, 0, TeraEditConstraint, NULL},
    {sizeof(struct RhhModHook), RHH_MON_EDITED, RHH_OBSERVE, 0, NULL, TeraEditReceipt}
};
static const struct RhhModDefinition sTeraEditMod = {.struct_size = sizeof(sTeraEditMod), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:tera_edit", .hooks = sTeraEditHooks, .hook_count = ARRAY_COUNT(sTeraEditHooks)};
static void TeraRequestRefresh(const struct RhhOwnedMonService *owned, struct RhhTeraEditRequest *request, bool32 box)
{
    request->edit.expected.struct_size = sizeof(request->edit.expected);
    CHECK(!(box ? owned->box_at(0, 0, &request->edit.expected) : owned->party_at(0, &request->edit.expected)));
    request->expected.struct_size = sizeof(request->expected);
    CHECK(!owned->tera_view(&request->edit.expected, &request->expected));
}
