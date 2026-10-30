#ifndef GUARD_BATTLE_TYPE_SET_H
#define GUARD_BATTLE_TYPE_SET_H

/* Battle types are byte-sized. Unlike a single word mask, this includes
 * dynamically assigned native types without shifting by 32 or more. */
#define BATTLE_TYPE_SET_CAPACITY 256u
/* Ordered, distinct runtime types. The byte-sized runtime type space bounds
 * the list even when a species or battler has more than three types. */
struct BattleTypeList
{
    u16 count;
    enum Type types[BATTLE_TYPE_SET_CAPACITY];
};

static inline void BattleTypeListAdd(struct BattleTypeList *list, enum Type type)
{
    if (type == TYPE_NONE || type == TYPE_MYSTERY || list->count >= BATTLE_TYPE_SET_CAPACITY)
        return;
    for (u32 i = 0; i < list->count; ++i)
        if (list->types[i] == type) return;
    list->types[list->count++] = type;
}

static inline bool32 BattleTypeListContains(const struct BattleTypeList *list, enum Type type)
{
    for (u32 i = 0; i < list->count; ++i)
        if (list->types[i] == type) return TRUE;
    return FALSE;
}

static inline bool32 BattleTypeListsEqual(const struct BattleTypeList *a, const struct BattleTypeList *b)
{
    if (a->count != b->count) return FALSE;
    for (u32 i = 0; i < a->count; ++i)
        if (a->types[i] != b->types[i]) return FALSE;
    return TRUE;
}

struct BattleTypeSet
{
    u32 words[BATTLE_TYPE_SET_CAPACITY / 32];
};

static inline void BattleTypeSetAdd(struct BattleTypeSet *set, u32 type)
{
    if (type < BATTLE_TYPE_SET_CAPACITY)
        set->words[type / 32] |= 1u << (type % 32);
}

static inline void BattleTypeSetAddSpecies(struct BattleTypeSet *set, const enum Type types[3])
{
    BattleTypeSetAdd(set, types[0]);
    BattleTypeSetAdd(set, types[1]);
    if (types[2] != TYPE_MYSTERY)
        BattleTypeSetAdd(set, types[2]);
}

static inline void BattleTypeCountsAddSpecies(u8 counts[BATTLE_TYPE_SET_CAPACITY], const enum Type types[3])
{
    for (u32 i = 0; i < 3; ++i)
    {
        u32 type = types[i];
        if ((i == 2 && type == TYPE_MYSTERY) || type >= BATTLE_TYPE_SET_CAPACITY
            || (i > 0 && type == types[0]) || (i > 1 && type == types[1]))
            continue;
        ++counts[type];
    }
}

static inline u32 BattleTypeSetCount(const struct BattleTypeSet *set)
{
    u32 count = 0;
    for (u32 i = 0; i < ARRAY_COUNT(set->words); ++i)
    {
        u32 word = set->words[i];
        while (word)
        {
            word &= word - 1;
            ++count;
        }
    }
    return count;
}

/* Ignore TYPE_NONE (index zero). The capacity is also the no-leader result. */
static inline u32 BattleTypeCountsUniqueLeader(const u8 counts[BATTLE_TYPE_SET_CAPACITY])
{
    u32 leader = BATTLE_TYPE_SET_CAPACITY, highest = 0;
    bool32 tied = FALSE;
    for (u32 i = 1; i < BATTLE_TYPE_SET_CAPACITY; ++i)
    {
        if (counts[i] > highest)
        {
            highest = counts[i];
            leader = i;
            tied = FALSE;
        }
        else if (highest && counts[i] == highest)
            tied = TRUE;
    }
    return tied ? BATTLE_TYPE_SET_CAPACITY : leader;
}

#endif
