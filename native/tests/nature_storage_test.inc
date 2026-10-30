#include "nature_storage.h"
#include "recorded_battle.h"
#include "util.h"
static void CheckNatureStorage(const char *path)
{
    _Static_assert(sizeof(struct PokemonSubstruct0) == 12, "nature codec must preserve the save layout");
    _Static_assert(sizeof(struct BoxPokemon) == 80, "nature codec must preserve boxed Pokémon size");
    struct Pokemon mon; struct BoxPokemon staged, original, corrupt, saved;
    for (u32 personality = 0; personality < 24; ++personality)
    {
        CreateMonWithIVs(&mon, SPECIES_TORCHIC, 20, personality, OTID_STRUCT_PRESET(54321), 17);
        u16 item = ITEM_LEFTOVERS; SetMonData(&mon, MON_DATA_HELD_ITEM, &item);
        SetMonMoveSlot(&mon, MOVE_TACKLE, 0); original = mon.box;
        rng_value_t rng = gRngValue, rng2 = gRng2Value;
        unsigned natural = GetNatureFromPersonality(personality), read = 99, overridden = 99;
        CHECK(!NativeNatureStorageRead(&original, &read, &overridden) && read == natural && !overridden);
        for (u32 nature = 0; nature < NUM_NATURES; ++nature)
        {
            staged = original;
            CHECK(!NativeNatureStorageWrite(&staged, nature)
                && !NativeNatureStorageRead(&staged, &read, &overridden) && read == nature && overridden == (nature != natural));
            CHECK(staged.personality == original.personality && staged.otId == original.otId
                && GetBoxMonData(&staged, MON_DATA_HELD_ITEM) == item && GetBoxMonData(&staged, MON_DATA_MOVE1) == MOVE_TACKLE
                && GetBoxMonData(&staged, MON_DATA_HP_IV) == 17 && GetBoxMonNature(&staged) == natural
                && GetBoxMonData(&staged, MON_DATA_IS_SHINY) == GetBoxMonData(&original, MON_DATA_IS_SHINY));
            struct BoxPokemon wire = original, received = original;
            unsigned required = 99;
            unsigned changed = nature != natural;
            CHECK(!NativeNatureLinkExport(&staged, 1, &wire) && !memcmp(&wire, &staged, sizeof(wire)));
            CHECK(!NativeNatureLinkImport(&wire, 1, &received, &required)
                && !memcmp(&received, &staged, sizeof(received)) && required == changed);
            wire = original;
            if (changed)
                CHECK(NativeNatureLinkExport(&staged, 0, &wire) && !memcmp(&wire, &original, sizeof(wire)));
            else
                CHECK(!NativeNatureLinkExport(&staged, 0, &wire) && !memcmp(&wire, &original, sizeof(wire)));
            required = 99; received = staged;
            if (changed)
                CHECK(NativeNatureLinkImport(&received, 0, &received, &required)
                    && required == 99 && !memcmp(&received, &staged, sizeof(received)));
            else
                CHECK(!NativeNatureLinkImport(&received, 0, &received, &required)
                    && !required && !memcmp(&received, &original, sizeof(received)));
            CHECK(!NativeNatureStorageWrite(&staged, natural) && !memcmp(&staged, &original, sizeof(staged)));
        }
        CHECK(!memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    }
    staged = original; unsigned target = (GetNatureFromPersonality(staged.personality) + 1) % NUM_NATURES;
    CHECK(!NativeNatureStorageWrite(&staged, target));
    FILE *file = fopen(path, "wb"); CHECK(file && fwrite(&staged, sizeof(staged), 1, file) == 1 && !fclose(file));
    memset(&saved, 0, sizeof(saved)); file = fopen(path, "rb");
    CHECK(file && fread(&saved, sizeof(saved), 1, file) == 1 && !fclose(file));
    unsigned read = 0, overridden = 0;
    CHECK(!memcmp(&saved, &staged, sizeof(saved)) && !NativeNatureStorageRead(&saved, &read, &overridden)
        && read == target && overridden);
    struct BoxPokemon clone = original;
    CHECK(!NativeNatureStorageWrite(&clone, (target + 1) % NUM_NATURES)
        && clone.personality == staged.personality && clone.otId == staged.otId
        && !NativeNatureStorageRead(&staged, &read, &overridden) && read == target
        && !NativeNatureStorageRead(&clone, &read, &overridden) && read == (target + 1) % NUM_NATURES);
    saved = staged; CHECK(NativeNatureStorageWrite(&staged, NUM_NATURES) && !memcmp(&staged, &saved, sizeof(staged)));
    corrupt = staged; corrupt.secure.raw[0] ^= 1; saved = corrupt; read = overridden = 99;
    CHECK(NativeNatureStorageRead(&corrupt, &read, &overridden) && read == 99 && overridden == 99
        && !memcmp(&corrupt, &saved, sizeof(corrupt)) && NativeNatureStorageWrite(&corrupt, 0)
        && !memcmp(&corrupt, &saved, sizeof(corrupt)));
    corrupt = staged; u8 badCode = NUM_NATURES + 1; SetBoxMonData(&corrupt, MON_DATA_NATIVE_NATURE_CODE, &badCode);
    saved = corrupt; read = overridden = 99;
    CHECK(NativeNatureStorageRead(&corrupt, &read, &overridden) && read == 99 && overridden == 99
        && NativeNatureStorageWrite(&corrupt, 0) && !memcmp(&corrupt, &saved, sizeof(corrupt)));
    struct BoxPokemon output = original; unsigned required = 99;
    CHECK(NativeNatureLinkImport(&corrupt, 1, &output, &required)
        && !memcmp(&output, &original, sizeof(output)) && required == 99);
    CHECK(NativeNatureLinkImport(&corrupt, 0, &output, &required)
        && required == 99 && !memcmp(&output, &original, sizeof(output)));
    corrupt.secure.raw[0] ^= 1; output = original; required = 99;
    CHECK(NativeNatureLinkImport(&corrupt, 0, &output, &required)
        && !memcmp(&output, &original, sizeof(output)) && required == 99);
    CHECK(NativeNatureLinkExport(&staged, 2, &output) && !memcmp(&output, &original, sizeof(output)));
    CHECK(NativeNatureLinkImport(&staged, -1, &output, &required)
        && !memcmp(&output, &original, sizeof(output)) && required == 99);
    struct Pokemon party[PARTY_SIZE] = {0}, converted[PARTY_SIZE], untouched[PARTY_SIZE];
    party[0] = mon; party[1] = mon; party[1].box = staged;
    memset(converted, 0xa5, sizeof(converted)); memcpy(untouched, converted, sizeof(converted));
    rng_value_t rng = gRngValue, rng2 = gRng2Value; required = 99;
    CHECK(!NativeNaturePartyLinkExport(party, PARTY_SIZE, 1, converted)
        && !memcmp(converted, party, sizeof(party)));
    memcpy(converted, untouched, sizeof(converted));
    CHECK(NativeNaturePartyLinkExport(party, PARTY_SIZE, 0, converted)
        && !memcmp(converted, untouched, sizeof(converted)));
    CHECK(!NativeNaturePartyLinkImport(party, PARTY_SIZE, 1, converted, &required)
        && required == 1 && !memcmp(converted, party, sizeof(party)));
    memcpy(converted, untouched, sizeof(converted)); required = 99;
    CHECK(NativeNaturePartyLinkImport(party, PARTY_SIZE, 0, converted, &required)
        && required == 99 && !memcmp(converted, untouched, sizeof(converted)));
    struct Pokemon inPlace[PARTY_SIZE]; memcpy(inPlace, party, sizeof(inPlace));
    CHECK(!NativeNaturePartyLinkExport(inPlace, PARTY_SIZE, 1, inPlace)
        && !NativeNaturePartyLinkImport(inPlace, PARTY_SIZE, 1, inPlace, &required)
        && required == 1 && !memcmp(inPlace, party, sizeof(inPlace)));
    struct Pokemon overlap[3] = {party[0], party[1], {0}};
    CHECK(!NativeNaturePartyLinkExport(overlap, 2, 1, overlap + 1)
        && !memcmp(overlap + 1, party, 2 * sizeof(*party)));
    party[1].box = original; required = 99;
    CHECK(!NativeNaturePartyLinkImport(party, PARTY_SIZE, 0, converted, &required)
        && !required && !memcmp(converted, party, sizeof(party)));
    party[1].box.secure.raw[0] ^= 1; memcpy(converted, untouched, sizeof(converted)); required = 99;
    CHECK(NativeNaturePartyLinkImport(party, PARTY_SIZE, 1, converted, &required)
        && required == 99 && !memcmp(converted, untouched, sizeof(converted))
        && NativeNaturePartyLinkExport(party, PARTY_SIZE, 1, converted)
        && !memcmp(converted, untouched, sizeof(converted)));
    party[1].box = original; u16 missingSpecies = SPECIES_NONE;
    SetBoxMonData(&party[1].box, MON_DATA_SPECIES, &missingSpecies); party[1].box.hasSpecies = TRUE;
    CHECK(NativeNaturePartyLinkExport(party, PARTY_SIZE, 1, converted)
        && NativeNaturePartyLinkImport(party, PARTY_SIZE, 1, converted, &required)
        && required == 99 && !memcmp(converted, untouched, sizeof(converted)));
    party[1].box = original; party[2].box.isBadEgg = TRUE;
    CHECK(NativeNaturePartyLinkExport(party, PARTY_SIZE, 1, converted)
        && NativeNaturePartyLinkImport(party, PARTY_SIZE, 1, converted, &required)
        && required == 99 && !memcmp(converted, untouched, sizeof(converted)));
    CHECK(NativeNaturePartyLinkExport(party, 0, 1, converted)
        && NativeNaturePartyLinkExport(party, PARTY_SIZE + 1, 1, converted)
        && NativeNaturePartyLinkImport(party, 1, 2, converted, &required)
        && required == 99 && !memcmp(converted, untouched, sizeof(converted))
        && !memcmp(&rng, &gRngValue, sizeof(rng)) && !memcmp(&rng2, &gRng2Value, sizeof(rng2)));
    struct RecordedBattleSave legacy = {0}, wire, decoded, untouchedRecording;
    legacy.battleFlags = BATTLE_TYPE_TRAINER; legacy.parties[0][0] = mon; legacy.parties[1][1] = mon;
    legacy.parties[1][1].box = staged;
    legacy.checksum = CalcByteArraySum((const u8 *)&legacy, sizeof(legacy) - sizeof(legacy.checksum));
    memset(&wire, 0xa5, sizeof(wire)); untouchedRecording = wire; required = 99;
    CHECK(!NativeNatureRecordingEncode(&legacy, 1, &wire)
        && (wire.battleFlags & BATTLE_TYPE_RECORDED_INVALID) && wire.padding
        && wire.checksum == CalcByteArraySum((const u8 *)&wire, sizeof(wire) - sizeof(wire.checksum)));
    CHECK(!NativeNatureRecordingDecode(&wire, &decoded, &required) && required == 1
        && !memcmp(&decoded, &legacy, sizeof(legacy)));
    decoded = wire;
    CHECK(!NativeNatureRecordingDecode(&decoded, &decoded, &required) && required == 1
        && !memcmp(&decoded, &legacy, sizeof(legacy)));
    CHECK(!NativeNatureRecordingEncode(&legacy, 0, &decoded) && !memcmp(&decoded, &legacy, sizeof(legacy))
        && !NativeNatureRecordingDecode(&decoded, &decoded, &required) && !required);
    decoded = untouchedRecording; required = 99;
    struct RecordedBattleSave invalidRecord = wire; invalidRecord.checksum ^= 1;
    CHECK(NativeNatureRecordingDecode(&invalidRecord, &decoded, &required)
        && required == 99 && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    invalidRecord = wire; invalidRecord.parties[1][1].box.secure.raw[0] ^= 1;
    invalidRecord.checksum = CalcByteArraySum((const u8 *)&invalidRecord, sizeof(invalidRecord) - sizeof(invalidRecord.checksum));
    CHECK(NativeNatureRecordingDecode(&invalidRecord, &decoded, &required)
        && required == 99 && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    invalidRecord = wire; invalidRecord.padding = 0;
    invalidRecord.checksum = CalcByteArraySum((const u8 *)&invalidRecord, sizeof(invalidRecord) - sizeof(invalidRecord.checksum));
    CHECK(NativeNatureRecordingDecode(&invalidRecord, &decoded, &required)
        && required == 99 && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    invalidRecord = legacy; u8 invalidNature = NUM_NATURES + 1;
    SetBoxMonData(&invalidRecord.parties[1][1].box, MON_DATA_NATIVE_NATURE_CODE, &invalidNature);
    invalidRecord.checksum = CalcByteArraySum((const u8 *)&invalidRecord, sizeof(invalidRecord) - sizeof(invalidRecord.checksum));
    CHECK(NativeNatureRecordingEncode(&invalidRecord, 1, &decoded)
        && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    invalidRecord = legacy; invalidRecord.padding = 1;
    invalidRecord.checksum = CalcByteArraySum((const u8 *)&invalidRecord, sizeof(invalidRecord) - sizeof(invalidRecord.checksum));
    CHECK(NativeNatureRecordingEncode(&invalidRecord, 1, &decoded)
        && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    CHECK(NativeNatureRecordingEncode(&legacy, -1, &decoded)
        && !memcmp(&decoded, &untouchedRecording, sizeof(decoded)));
    puts("nature storage: staged codec, legacy defaults, checksum and serialized roundtrip passed");
}
