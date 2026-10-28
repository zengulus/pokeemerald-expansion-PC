#ifndef RHH_MOD_HOST_H
#define RHH_MOD_HOST_H
#include "mod_api.h"
#include <stdio.h>

struct RhhContentBinding { const char *name; uint32_t kind, engine_id; };
void RhhModsInit(void);
void RhhModsShutdown(void);
int RhhModsRegister(const struct RhhModDefinition *);
int RhhModsFinalize(void);
const char *RhhModsError(void);
void RhhModsSetTrace(FILE *);
void RhhModsSetContentMetadata(void (*provider)(uint32_t kind, uint32_t engine_id, struct RhhContentInfo *));
int RhhModsSetContent(const struct RhhContentBinding *, size_t count);
RhhContentKey RhhContentKeyOf(const char *);
int RhhContentResolve(uint32_t kind, RhhContentKey, uint32_t *engine_id);
const char *RhhContentName(uint32_t kind, uint32_t engine_id);
RhhContentKey RhhContentKeyForId(uint32_t kind, uint32_t engine_id);
uint32_t RhhContentCount(uint32_t kind);
int RhhContentAt(uint32_t kind, uint32_t index, struct RhhContentInfo *out);
int RhhContentInfoFor(uint32_t kind, RhhContentKey key, struct RhhContentInfo *out);
int RhhModsGetSetting(const char *mod_id, const char *setting_id, struct RhhModValue *out);
int RhhModsConfigure(const char *mod_id, const char *setting_id, const struct RhhModValue *value);
uint64_t RhhModRandomAt(uint64_t seed, const char *mod_id, uint64_t key, uint64_t ordinal);
const struct RhhModHostV1 *RhhModsHostAPI(void);
void *RhhModsState(const char *id); /* host adapter only; modules receive borrowed state */
void RhhModsResetState(void);
void RhhModsDispatch(const struct RhhModEvent *, struct RhhModDecision *);
/* Bounded, checksummed, little-endian records. Caller owns exported buffer. */
int RhhModsExport(uint8_t **data, uint32_t *size);
int RhhModsImport(const uint8_t *data, uint32_t size);
int RhhModsMergeJournal(const uint8_t *data, uint32_t size);
void RhhModsSetPersistence(int (*persist)(uint64_t run_id, const uint8_t *data, uint32_t size));
void RhhModsSetRunContext(uint64_t seed, uint64_t run_id, int committed);
int RhhModsFlushDurable(void);
int RhhModsLoadLibrary(const char *path);
void RhhModsUnloadLibraries(void);

#endif
