#ifndef RHH_NATURE_STORAGE_H
#define RHH_NATURE_STORAGE_H

struct BoxPokemon;
struct Pokemon;
struct RecordedBattleSave;
/* Staging-only codec. Do not persist or transfer its output until the save
 * version and link compatibility policy are enforced by the host. */
int NativeNatureStorageRead(const struct BoxPokemon *box, unsigned *nature, unsigned *overridden);
int NativeNatureStorageWrite(struct BoxPokemon *box, unsigned nature);
/* Staged link conversion: callers must negotiate peer support and apply the
 * save-format marker before committing an import that requires it. Legacy
 * upper item bits cannot be represented safely and are rejected. Both
 * functions leave their outputs unchanged on failure and allow in-place use.
 * requires_format must not overlap either Pokémon. */
int NativeNatureLinkExport(const struct BoxPokemon *source, int peer_supports_format, struct BoxPokemon *out);
int NativeNatureLinkImport(const struct BoxPokemon *source, int peer_supports_format,
                           struct BoxPokemon *out, unsigned *requires_format);
/* Convert one through PARTY_SIZE full Pokémon as a single operation. Empty
 * slots are copied unchanged; a bad egg, invalid occupied slot or any failed
 * conversion rejects the entire batch. Output may overlap input, but the
 * requires_format pointer must not overlap either array. These are staging
 * helpers only; callers must negotiate support and commit the save marker. */
int NativeNaturePartyLinkExport(const struct Pokemon *source, unsigned count, int peer_supports_format,
                                struct Pokemon *out);
int NativeNaturePartyLinkImport(const struct Pokemon *source, unsigned count, int peer_supports_format,
                                struct Pokemon *out, unsigned *requires_format);
/* Versioned recording conversion. The nature format uses reserved invalid
 * battle flags so older readers reject it. The decoded copy has ordinary
 * battle flags restored. Outputs are unchanged on failure; source and out
 * may be the same object, but requires_format must not overlap either. */
int NativeNatureRecordingEncode(const struct RecordedBattleSave *source, int nature_format,
                                struct RecordedBattleSave *out);
int NativeNatureRecordingDecode(const struct RecordedBattleSave *source, struct RecordedBattleSave *out,
                                unsigned *requires_format);
/* -1 outside playback, 0 for a legacy recording, 1 for the nature format. */
int NativeRecordedBattleNatureMode(void);
#endif
