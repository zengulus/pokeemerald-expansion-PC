#ifndef GUARD_HALL_OF_FAME_H
#define GUARD_HALL_OF_FAME_H

struct HallofFameMon
{
    u32 tid;
    u32 personality;
    u16 isShiny:1;
    enum Species species:15;
    u8 lvl;
    u8 nickname[POKEMON_NAME_LENGTH];
};

struct HallofFameTeam
{
    struct HallofFameMon mon[PARTY_SIZE];
};

#ifdef NATIVE
extern size_t gHoFSaveCount;
bool32 NativeHallOfFameAppend(const struct HallofFameTeam *team);
#define HALL_OF_FAME_STORAGE_SIZE (sizeof(struct HallofFameTeam) * gHoFSaveCount)
#else
#define HALL_OF_FAME_MAX_TEAMS 30
#define HALL_OF_FAME_STORAGE_SIZE (SECTOR_SIZE * NUM_HOF_SECTORS)
#endif

extern struct HallofFameTeam *gHoFSaveBuffer;

void CB2_DoHallOfFameScreen(void);
void CB2_DoHallOfFameScreenDontSaveData(void);
void CB2_DoHallOfFamePC(void);

// hof_pc.c
void ReturnFromHallOfFamePC(void);

#endif // GUARD_HALL_OF_FAME_H
