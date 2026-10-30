#include "mod_api.h"
#include <stdio.h>
#include <string.h>
static const struct RhhUiAccessibilityService *access;
static void Observe(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhUiAccessibleSnapshot snapshot = {.struct_size = sizeof(snapshot)};
    struct RhhUiAccessibleContent content = {.struct_size = sizeof(content), .title = "Invalid"};
    int overlay = event->kind == RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED;
    if ((overlay ? access->overlay_snapshot(&snapshot) : access->snapshot(&snapshot))
        || snapshot.revision != event->ordinal || snapshot.active != event->args[0]
        || snapshot.owner != event->subject || !access->publish(snapshot.owner, &content)
        || (snapshot.active && (strcmp(snapshot.content.title, "Pok\xc3\xa9mon stats")
             || strcmp(snapshot.content.body, "HP 31\nAttack 12"))))
        puts("A11Y_ADAPTER_BAD");
    else if (overlay) puts(snapshot.active ? "A11Y_OVERLAY_CONTENT" : "A11Y_OVERLAY_CLOSED");
    else puts(snapshot.active ? "A11Y_ADAPTER_CONTENT" : "A11Y_ADAPTER_CLOSED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_UI_ACCESSIBILITY_CHANGED, RHH_OBSERVE, 0, NULL, Observe},
    {sizeof(struct RhhModHook), RHH_UI_OVERLAY_ACCESSIBILITY_CHANGED, RHH_OBSERVE, 0, NULL, Observe}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 1), RHH_SERVICE_REQUIRED, 0, "rhh:service/ui_accessibility"}
};
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:a11y_adapter", .hooks = hooks, .hook_count = 2, .service_requirements = requirements, .service_requirement_count = 1};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    access = (const void *)host->get_service("rhh:service/ui_accessibility", RHH_SERVICE_VERSION(1, 1));
    return access ? &definition : NULL;
}
