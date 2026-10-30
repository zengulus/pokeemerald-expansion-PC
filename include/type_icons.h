#ifndef GUARD_TYPE_ICONS_H
#define GUARD_TYPE_ICONS_H

void LoadTypeIcons(enum BattlerId battler);
#ifdef NATIVE
struct BattleTypeList;
void GetBattlerPublicTypeList(enum BattlerId battler, struct BattleTypeList *types);
u32 GetBattlerPublicTypeIcons(enum BattlerId battler, enum Type types[3]);
#endif

#define TYPE_ICON_TAG 0x2720
#define TYPE_ICON_TAG_2 0x2721
#define NUM_FRAMES_HIDE_TYPE_ICON 10

#define tMonPosition      data[0]
#define tBattlerId        data[1]
#define tHideIconTimer    data[2]
#define tVerticalPosition data[3]
#define tTypeIconSlot     data[4]
#define tTypeIconTimer    data[5]
#define tTypeIconPage     data[6]
#define tTypeIconType     data[7]

#define TYPE_ICON_1_FRAME(monType) ((monType - 1) * 2)
#define TYPE_ICON_2_FRAME(monType) ((monType - 11) * 2)

#endif // GUARD_TYPE_ICONS_H
