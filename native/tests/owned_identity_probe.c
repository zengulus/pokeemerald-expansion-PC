#include "mod_api.h"
#include <stdio.h>
#include <string.h>
/* Independently compiled old and new prefixes exercise ABI compatibility. */
struct OwnedServiceV11 {
    struct RhhServiceHeader header;
    uint32_t (*party_slots)(void), (*box_count)(void), (*box_slots)(void);
    int (*party_at)(uint32_t, struct RhhOwnedMonSnapshot *);
    int (*box_at)(uint32_t, uint32_t, struct RhhOwnedMonSnapshot *);
    int (*find)(uint64_t, struct RhhOwnedMonSnapshot *);
    int (*change_context)(struct RhhOwnedMonChange *);
};
_Static_assert(sizeof(struct OwnedServiceV11) == RHH_OWNED_MON_SERVICE_V1_1_SIZE, "owned service v1.1 prefix changed");
struct OwnedServiceV12 {
    struct OwnedServiceV11 v11;
    int (*identity_query)(uint64_t, struct RhhOwnedIdentityQuery *);
};
_Static_assert(sizeof(struct OwnedServiceV12) == RHH_OWNED_MON_SERVICE_V1_2_SIZE, "owned service v1.2 prefix changed");
struct OwnedServiceV13 {
    struct OwnedServiceV12 v12;
    uint32_t (*daycare_count)(void), (*daycare_slots)(uint32_t), (*fusion_slots)(void);
    int (*daycare_at)(uint32_t, uint32_t, struct RhhOwnedMonSnapshot *);
    int (*fusion_at)(uint32_t, struct RhhOwnedMonSnapshot *);
};
_Static_assert(sizeof(struct OwnedServiceV13) == RHH_OWNED_MON_SERVICE_V1_3_SIZE, "owned service v1.3 prefix changed");
struct OwnedServiceV14 { struct OwnedServiceV13 v13; int (*release_context)(struct RhhOwnedRelease *); };
_Static_assert(sizeof(struct OwnedServiceV14) == RHH_OWNED_MON_SERVICE_V1_4_SIZE, "owned service v1.4 prefix changed");
struct OwnedServiceV15 { struct OwnedServiceV14 v14; int (*move_context)(struct RhhOwnedMove *); };
_Static_assert(sizeof(struct OwnedServiceV15) == RHH_OWNED_MON_SERVICE_V1_5_SIZE, "owned service v1.5 prefix changed");
struct OwnedServiceV16 { struct OwnedServiceV15 v15; int (*party_reorder_context)(struct RhhPartyReorder *); };
_Static_assert(sizeof(struct OwnedServiceV16) == RHH_OWNED_MON_SERVICE_V1_6_SIZE, "owned service v1.6 prefix changed");
struct OwnedServiceV17 { struct OwnedServiceV16 v16; int (*daycare_transfer_context)(struct RhhDaycareTransfer *); };
_Static_assert(sizeof(struct OwnedServiceV17) == RHH_OWNED_MON_SERVICE_V1_7_SIZE, "owned service v1.7 prefix changed");
struct OwnedServiceV18 { struct OwnedServiceV17 v17; int (*fusion_transfer_context)(struct RhhFusionTransfer *); };
_Static_assert(sizeof(struct OwnedServiceV18) == RHH_OWNED_MON_SERVICE_V1_8_SIZE, "owned service v1.8 prefix changed");
struct OwnedServiceV19 { struct OwnedServiceV18 v18; uint32_t (*party_view)(void); };
_Static_assert(sizeof(struct OwnedServiceV19) == RHH_OWNED_MON_SERVICE_V1_9_SIZE, "owned service v1.9 prefix changed");
struct OwnedServiceV110 { struct OwnedServiceV19 v19; int (*catch_placement_context)(struct RhhCatchPlacement *); };
_Static_assert(sizeof(struct OwnedServiceV110) == RHH_OWNED_MON_SERVICE_V1_10_SIZE, "owned service v1.10 prefix changed");
struct OwnedServiceV111 { struct OwnedServiceV110 v110; int (*ingame_trade_context)(struct RhhIngameTrade *); };
_Static_assert(sizeof(struct OwnedServiceV111) == RHH_OWNED_MON_SERVICE_V1_11_SIZE, "owned service v1.11 prefix changed");
struct OwnedServiceV112 { struct OwnedServiceV111 v111; int (*link_trade_context)(struct RhhLinkTrade *); uint32_t (*trade_state)(void); };
_Static_assert(sizeof(struct OwnedServiceV112) == RHH_OWNED_MON_SERVICE_V1_12_SIZE, "owned service v1.12 prefix changed");
struct OwnedServiceV113 {
    struct OwnedServiceV112 v112;
    uint32_t (*link_trade_history_count)(void);
    int (*link_trade_history_at)(uint32_t, struct RhhSavedLinkTrade *);
    int (*link_trade_receipt_context)(struct RhhSavedLinkTrade *);
};
_Static_assert(sizeof(struct OwnedServiceV113) == RHH_OWNED_MON_SERVICE_V1_13_SIZE, "owned service v1.13 prefix changed");
struct OwnedServiceV114 { struct OwnedServiceV113 v113;
    int (*link_trade_checkpoint)(uint32_t, struct RhhLinkHistoryCheckpoint *);
    int (*link_trade_reconcile)(const struct RhhLinkHistoryCheckpoint *, struct RhhLinkHistoryReconcile *); };
_Static_assert(sizeof(struct OwnedServiceV114) == RHH_OWNED_MON_SERVICE_V1_14_SIZE, "owned service v1.14 prefix changed");
struct OwnedServiceV115 { struct OwnedServiceV114 v114; int (*pickup_context)(struct RhhOwnedPickup *); };
_Static_assert(sizeof(struct OwnedServiceV115) == RHH_OWNED_MON_SERVICE_V1_15_SIZE, "owned service v1.15 prefix changed");
struct OwnedServiceV116 { struct OwnedServiceV115 v115; int (*storage_unreadable_context)(struct RhhOwnedStorageUnreadable *); };
_Static_assert(sizeof(struct OwnedServiceV116) == RHH_OWNED_MON_SERVICE_V1_16_SIZE, "owned service v1.16 prefix changed");
struct OwnedServiceV117 { struct OwnedServiceV116 v116; int (*gift_context)(struct RhhOwnedGift *); };
_Static_assert(sizeof(struct OwnedServiceV117) == RHH_OWNED_MON_SERVICE_V1_17_SIZE, "owned service v1.17 prefix changed");
struct OwnedServiceV118 { struct OwnedServiceV117 v117; int (*evolution_context)(struct RhhOwnedEvolution *); };
_Static_assert(sizeof(struct OwnedServiceV118) == RHH_OWNED_MON_SERVICE_V1_18_SIZE, "owned service v1.18 prefix changed");
struct OwnedServiceV119 { struct OwnedServiceV118 v118; int (*battle_view_field_slot)(uint32_t, uint32_t *); };
_Static_assert(sizeof(struct OwnedServiceV119) == RHH_OWNED_MON_SERVICE_V1_19_SIZE, "owned service v1.19 prefix changed");
struct OwnedServiceV120 { struct OwnedServiceV119 v119; int (*sky_party_field_slot)(uint32_t, uint32_t *); };
_Static_assert(sizeof(struct OwnedServiceV120) == RHH_OWNED_MON_SERVICE_V1_20_SIZE, "owned service v1.20 prefix changed");
static const struct OwnedServiceV11 *old;
static const struct RhhOwnedMonService *owned;
static void EvolutionCommitted(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned count;
    struct RhhOwnedEvolution evolution = {.struct_size = sizeof(evolution)};
    struct RhhOwnedEvolution small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    unsigned cause = count == 1 || count == 2 ? RHH_EVOLUTION_SPLIT : RHH_EVOLUTION_CHANGED;
    unsigned flags = RHH_EVOLUTION_BEFORE_VALID | RHH_EVOLUTION_AFTER_VALID
        | (count == 2 ? RHH_EVOLUTION_REPLACED : 0)
        | (count >= 3 ? RHH_EVOLUTION_TRADE : 0)
        | (count == 3 || count == 5 ? RHH_EVOLUTION_LINK_TRADE : 0)
        | (count == 4 ? RHH_EVOLUTION_PC_PENDING : 0);
    unsigned source = count == 4 ? RHH_MON_STAGED : RHH_MON_PARTY;
    unsigned dest = count == 4 ? RHH_MON_BOX : RHH_MON_PARTY;
    unsigned sourceSlot = count == 4 ? 1 : 0;
    unsigned destSlot = count == 1 || count == 2 ? 1 : 0;
    if (count >= 6 || owned->evolution_context(&evolution) || !owned->evolution_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || evolution.run_id != 456 || evolution.sequence != count
        || event->ordinal != count || evolution.cause != cause || evolution.flags != flags
        || evolution.source_location != source || evolution.source_slot != sourceSlot
        || evolution.destination_location != dest || evolution.destination_box || evolution.destination_slot != destSlot
        || evolution.before.location != source || evolution.before.slot != sourceSlot
        || evolution.after.location != (count == 4 ? RHH_MON_STAGED : dest)
        || evolution.after.slot != (count == 4 ? sourceSlot : destSlot)
        || evolution.before.identity != evolution.after.identity || evolution.before.species == evolution.after.species
        || event->subject != evolution.before.identity || event->event_key != evolution.after.species
        || event->args[0] != cause || event->args[1] != flags || event->args[2] != dest || event->args[3]
        || !owned->identity_query(evolution.after.identity, &query)
        || (count < 3 && (old->party_at(destSlot, &actual) || actual.species != evolution.after.species))
        || ((count == 3 || count == 5) && !old->party_at(destSlot, &actual))
        || (count == 4 && (old->box_at(0, 0, &actual) || actual.species != evolution.before.species))
        || (count == 2 && (evolution.replaced.identity || evolution.replaced.species)))
    { printf("OWNED_IDENTITY_BAD evolution count=%u cause=%u flags=%u src=%u/%u dst=%u/%u/%u\n", count,
             evolution.cause, evolution.flags, evolution.source_location, evolution.source_slot,
             evolution.destination_location, evolution.destination_box, evolution.destination_slot); return; }
    ++count; puts("OWNED_EVOLUTION_OBSERVED");
}
static void GiftReceived(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned count;
    struct RhhOwnedGift gift = {.struct_size = sizeof(gift)};
    struct RhhOwnedGift small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    unsigned cause = count >= 4 ? RHH_GIFT_OWNER_ASSIGNED : RHH_GIFT_SCRIPTED;
    unsigned location = count == 3 || count == 5 ? RHH_MON_BOX : RHH_MON_PARTY;
    unsigned slot = count == 2 || count == 5 ? 1 : count == 4 ? 5 : 0;
    unsigned flags = RHH_GIFT_BEFORE_VALID | RHH_GIFT_AFTER_VALID
        | (count == 1 || count == 2 ? RHH_GIFT_REPLACED : 0)
        | (count == 1 ? RHH_GIFT_REPLACED_VALID : 0);
    if (count >= 6 || owned->gift_context(&gift) || !owned->gift_context(&small) || memcmp(&small, &saved, sizeof(small))
        || gift.run_id != 456 || gift.sequence != count || event->ordinal != count || gift.cause != cause || gift.flags != flags
        || gift.destination_location != location || gift.destination_box || gift.destination_slot != slot
        || event->args[0] != cause || event->args[1] != flags || event->args[2] != location || event->args[3]
        || gift.before.location != RHH_MON_STAGED || gift.after.location != location || gift.after.slot != slot
        || gift.before.species != gift.after.species || event->event_key != gift.after.species || event->subject != gift.before.identity
        || !owned->identity_query(gift.after.identity, &query)
        || (location == RHH_MON_PARTY ? old->party_at(slot, &actual) : old->box_at(0, slot, &actual))
        || actual.identity != gift.after.identity || (count >= 4 && (!gift.before.is_egg || !gift.after.is_egg))
        || (count == 1 && (gift.before.ivs[RHH_STAT_HP] != 27 || gift.replaced.location != RHH_MON_PARTY))
        || (count == 2 && (gift.replaced.identity || gift.replaced.species)))
    { printf("OWNED_IDENTITY_BAD gift count=%u cause=%u flags=%u dest=%u/%u/%u\n", count, gift.cause, gift.flags,
             gift.destination_location, gift.destination_box, gift.destination_slot); return; }
    ++count; puts("OWNED_GIFT_OBSERVED");
}
static void DebugCreated(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned count;
    struct RhhOwnedGift gift = {.struct_size = sizeof(gift)}, small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    if (count >= 2 || owned->gift_context(&gift) || !owned->gift_context(&small) || memcmp(&small, &saved, sizeof(small))
        || gift.run_id != 456 || gift.sequence != count || event->ordinal != count || gift.cause != RHH_GIFT_DEBUG
        || gift.flags != (RHH_GIFT_BEFORE_VALID | RHH_GIFT_AFTER_VALID)
        || gift.destination_location != RHH_MON_BOX || gift.destination_box || gift.destination_slot != 4 + count
        || gift.before.location != RHH_MON_STAGED || gift.after.location != RHH_MON_BOX
        || gift.before.identity != gift.after.identity || event->subject != gift.before.identity
        || event->event_key != gift.after.species || event->args[0] != RHH_GIFT_DEBUG
        || event->args[1] != gift.flags || event->args[2] != RHH_MON_BOX || event->args[3]
        || old->box_at(0, 4 + count, &actual) || actual.identity != gift.after.identity)
    { puts("OWNED_IDENTITY_BAD debug creation"); return; }
    ++count; puts("OWNED_DEBUG_CREATED_OBSERVED");
}
static void MysteryGift(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    struct RhhOwnedGift gift = {.struct_size = sizeof(gift)}, small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    if (owned->gift_context(&gift) || !owned->gift_context(&small) || memcmp(&small, &saved, sizeof(small))
        || gift.run_id != 456 || gift.sequence || event->ordinal || gift.cause != RHH_GIFT_MYSTERY_EVENT
        || gift.flags != (RHH_GIFT_BEFORE_VALID | RHH_GIFT_AFTER_VALID)
        || gift.destination_location != RHH_MON_PARTY || gift.destination_box || gift.destination_slot != 5
        || gift.before.location != RHH_MON_STAGED || gift.after.location != RHH_MON_PARTY || gift.after.slot != 5
        || gift.before.identity != gift.after.identity || event->subject != gift.before.identity
        || event->event_key != gift.after.species || event->args[0] != RHH_GIFT_MYSTERY_EVENT
        || event->args[1] != gift.flags || event->args[2] != RHH_MON_PARTY || event->args[3]
        || !old->party_at(5, &actual))
    { puts("OWNED_IDENTITY_BAD mystery gift"); return; }
    puts("OWNED_MYSTERY_GIFT_OBSERVED");
}
static void PickedUp(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned count;
    static const unsigned locations[] = {RHH_MON_BOX, RHH_MON_PARTY, RHH_MON_PARTY, RHH_MON_BOX,
                                         RHH_MON_BOX, RHH_MON_BOX, RHH_MON_BOX, RHH_MON_BOX,
                                         RHH_MON_BOX, RHH_MON_BOX, RHH_MON_BOX, RHH_MON_BOX};
    static const unsigned boxes[] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
    static const unsigned slots[] = {0, 0, 0, 1, 0, 1, 6, 2, 2, 3, 0, 5};
    struct RhhOwnedPickup pickup = {.struct_size = sizeof(pickup)};
    struct RhhOwnedPickup small = {.struct_size = 1, .source_slot = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    if (count >= sizeof(slots) / sizeof(slots[0]) || owned->pickup_context(&pickup)
        || !owned->pickup_context(&small) || memcmp(&small, &saved, sizeof(small))
        || pickup.run_id != 456 || pickup.sequence != count || event->ordinal != count
        || pickup.source_location != locations[count] || pickup.source_box != boxes[count] || pickup.source_slot != slots[count]
        || pickup.flags != ((count == 4 || count == 5 || count == 10 ? RHH_OWNED_PICKUP_GROUP : 0)
                            | (count >= 7 ? 0 : RHH_OWNED_PICKUP_VALID))
        || event->args[0] != pickup.flags || event->args[1] != locations[count]
        || event->args[2] != boxes[count] || event->args[3] != slots[count]
        || (count < 7 && (pickup.mon.identity != (((uint64_t)54321 << 32) | 12345)
                           || pickup.mon.location != locations[count] || pickup.mon.box != boxes[count]
                           || pickup.mon.slot != slots[count]))
        || (count >= 7 && (pickup.mon.identity || pickup.mon.species))
        || !owned->identity_query(((uint64_t)54321 << 32) | 12345, &query)
        || !(locations[count] == RHH_MON_PARTY ? old->party_at(slots[count], &actual)
                                               : old->box_at(boxes[count], slots[count], &actual)))
    { printf("OWNED_IDENTITY_BAD pickup count=%u flags=%u source=%u/%u/%u\n", count, pickup.flags,
             pickup.source_location, pickup.source_box, pickup.source_slot); return; }
    ++count; puts("OWNED_PICKUP_OBSERVED");
}
static void StorageUnreadable(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; (void)state;
    static unsigned count;
    struct RhhOwnedStorageUnreadable context = {.struct_size = sizeof(context)};
    struct RhhOwnedStorageUnreadable small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    unsigned release = count == 0 || count == 1 || count == 4 || count == 6;
    unsigned cause = release ? RHH_OWNED_STORAGE_UNREADABLE_RELEASE : RHH_OWNED_STORAGE_UNREADABLE_MOVE;
    if (count >= 8 || owned->storage_unreadable_context(&context) || !owned->storage_unreadable_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || context.run_id != 456 || context.sequence != count
        || event->ordinal != count || context.cause != cause || event->args[0] != cause || event->args[2] || event->args[3]
        || !owned->identity_query(((uint64_t)54321 << 32) | 12345, &query)) goto bad;
    if (release)
    {
        struct RhhOwnedRelease *r = &context.release;
        unsigned flags = RHH_OWNED_RELEASE_UNREADABLE | (count == 6 ? 0 : RHH_OWNED_RELEASE_MOVING);
        if (r->flags != flags || event->args[1] != flags || r->source_location != RHH_MON_BOX || r->source_box
            || r->source_slot != (count == 4 ? 4u : 2u) || r->mon.identity || r->mon.species) goto bad;
    }
    else
    {
        struct RhhOwnedMove *m = &context.move;
        unsigned flags = RHH_OWNED_MOVE_BEFORE_UNREADABLE | RHH_OWNED_MOVE_AFTER_UNREADABLE
            | (count == 3 ? RHH_OWNED_MOVE_DISPLACED | RHH_OWNED_MOVE_DISPLACED_UNREADABLE : 0)
            | (count == 7 ? RHH_OWNED_MOVE_DISPLACED : 0)
            | (count == 5 ? RHH_OWNED_MOVE_GROUP : 0);
        if (m->flags != flags || event->args[1] != flags || m->source_location != RHH_MON_BOX || m->source_box
            || m->source_slot != (count == 2 ? 2u : count == 3 ? 3u : count == 7 ? 5u : 0u)
            || m->destination_location != RHH_MON_BOX || m->destination_box != (count == 5 ? 1u : 0u)
            || m->destination_slot != (count == 2 ? 3u : count == 3 || count == 7 ? 4u : 6u)
            || m->before.identity || m->after.identity
            || (count == 7 && m->displaced.identity != (((uint64_t)54321 << 32) | 12345))) goto bad;
    }
    ++count; puts("OWNED_STORAGE_UNREADABLE_OBSERVED"); return;
bad:
    printf("OWNED_IDENTITY_BAD unreadable count=%u cause=%u flags=%u\n", count, context.cause, event->args[1]);
}
static void Update(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; (void)state;
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon)};
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedIdentityOccurrences occurrences = {.struct_size = sizeof(occurrences)};
    if (old->party_at(0, &mon) || owned->identity_query(mon.identity, &query)
        || query.matches != 1 || !query.unique_valid || query.unique.identity != mon.identity
        || owned->identity_occurrences(mon.identity, &occurrences) || occurrences.identity != mon.identity
        || occurrences.matches != 1 || occurrences.count != 1 || occurrences.flags
        || occurrences.entries[0].location != RHH_MON_PARTY || occurrences.entries[0].slot != 0
        || occurrences.entries[0].species != mon.species || occurrences.entries[0].flags)
        puts("OWNED_IDENTITY_BAD");
    else puts("OWNED_IDENTITY_PROBE_OK");
}
static void Released(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision;
    unsigned char *data = state;
    struct RhhOwnedRelease release = {.struct_size = sizeof(release)};
    struct RhhOwnedRelease small = {.struct_size = 1, .source_slot = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedMonSnapshot source = {.struct_size = sizeof(source)};
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    if (data[0] == 5)
    {
        if (owned->release_context(&release) || release.run_id != 456 || release.sequence != data[1]
            || release.flags != RHH_OWNED_RELEASE_MOVING || release.source_location != RHH_MON_BOX
            || release.source_box || release.source_slot != 4 || release.mon.identity != identity
            || release.mon.location != RHH_MON_MOVING || !old->box_at(0, 4, &source))
        { puts("OWNED_IDENTITY_BAD release valid displaced"); return; }
        ++data[1]; puts("OWNED_RELEASE_OBSERVED"); return;
    }
    if (owned->release_context(&release) || !owned->release_context(&small) || memcmp(&small, &saved, sizeof(small))
        || release.run_id != 456 || release.sequence != data[1] || event->ordinal != release.sequence
        || release.mon.identity != identity || event->subject != identity || event->event_key != release.mon.species
        || release.source_slot || release.source_box || event->args[0] != release.flags
        || event->args[1] != release.source_location || event->args[2] || event->args[3]
        || release.source_location != (data[0] == 1 || data[0] == 4 ? RHH_MON_PARTY : RHH_MON_BOX)
        || release.flags != (data[0] >= 3 ? RHH_OWNED_RELEASE_MOVING : 0)
        || release.mon.location != (data[0] >= 3 ? RHH_MON_MOVING : release.source_location)
        || !owned->identity_query(identity, &query)
        || (data[0] == 1 && !old->party_at(0, &source))
        || (data[0] == 2 && !old->box_at(0, 0, &source))
        || (data[0] == 3 && (old->box_at(0, 0, &source) || source.identity == identity))
        || (data[0] == 4 && (old->party_at(0, &source) || source.identity == identity)))
    {
        printf("OWNED_IDENTITY_BAD release mode=%u count=%u run=%llu seq=%llu id=%llu flags=%u source=%u/%u/%u monloc=%u ev=%u/%u/%u/%u query=%d\n",
               data[0], data[1], (unsigned long long)release.run_id, (unsigned long long)release.sequence,
               (unsigned long long)release.mon.identity, release.flags, release.source_location, release.source_box,
               release.source_slot, release.mon.location, event->args[0], event->args[1], event->args[2], event->args[3],
               owned->identity_query(identity, &query));
        return;
    }
    ++data[1]; puts("OWNED_RELEASE_OBSERVED");
}
static void Moved(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned mode = data[2], count = data[3];
    struct RhhOwnedMove move = {.struct_size = sizeof(move)};
    struct RhhOwnedMove small = {.struct_size = 1, .source_slot = 999}, saved = small;
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    unsigned sourceLocation = mode == 1 ? RHH_MON_PARTY : RHH_MON_BOX;
    unsigned sourceBox = mode == 3 || mode == 5 ? 1 : 0;
    unsigned sourceSlot = mode == 1 ? 0 : mode == 2 ? 1 : mode == 3 ? 2 : mode == 5 ? 6 : count - 3;
    unsigned destinationBox = mode == 2 || mode == 4 ? 1 : 0;
    unsigned destinationSlot = mode == 2 || mode == 5 ? 2 : mode == 4 ? 6 + count - 3 : 1;
    unsigned flags = mode == 2 ? RHH_OWNED_MOVE_DISPLACED : mode == 4 ? RHH_OWNED_MOVE_GROUP
        : mode == 5 ? RHH_OWNED_MOVE_DISPLACED | RHH_OWNED_MOVE_DISPLACED_UNREADABLE : 0;
    unsigned iv = mode == 3 || (mode == 4 && count == 4) ? 27 : 31;
    if (owned->move_context(&move) || !owned->move_context(&small) || memcmp(&small, &saved, sizeof(small))
        || move.run_id != 456 || move.sequence != count || event->ordinal != count || move.flags != flags
        || event->args[0] != flags || event->args[1] != sourceLocation || event->args[2] != RHH_MON_BOX
        || move.source_location != sourceLocation || move.source_box != sourceBox || move.source_slot != sourceSlot
        || move.destination_location != RHH_MON_BOX || move.destination_box != destinationBox || move.destination_slot != destinationSlot
        || move.before.identity != identity || move.after.identity != identity || event->subject != identity
        || move.before.location != RHH_MON_MOVING || move.after.location != RHH_MON_BOX
        || move.before.ivs[RHH_STAT_HP] != iv || move.after.ivs[RHH_STAT_HP] != iv
        || old->box_at(destinationBox, destinationSlot, &actual) || actual.ivs[RHH_STAT_HP] != iv
        || !owned->identity_query(identity, &query)
        || (mode == 2 && (move.displaced.identity != identity || move.displaced.ivs[RHH_STAT_HP] != 27)))
    { printf("OWNED_IDENTITY_BAD move mode=%u count=%u source=%u/%u/%u dest=%u/%u/%u flags=%u\n",
             mode, count, move.source_location, move.source_box, move.source_slot,
             move.destination_location, move.destination_box, move.destination_slot, move.flags); return; }
    ++data[3]; puts("OWNED_MOVE_OBSERVED");
}
static void Reordered(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned mode = data[4], count = data[5];
    struct RhhPartyReorder batch = {.struct_size = sizeof(batch)};
    struct RhhPartyReorder small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    if (owned->party_reorder_context(&batch) || !owned->party_reorder_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || batch.run_id != 456 || batch.sequence != count
        || event->ordinal != count || batch.count != (mode >= 3 ? 1u : 2u)
        || batch.cause != (mode == 1 ? RHH_PARTY_REORDER_SWAP : RHH_PARTY_REORDER_COMPACT)
        || event->args[0] != batch.cause || event->args[1] != batch.count || event->args[2] || event->args[3]
        || !owned->identity_query(((uint64_t)54321 << 32) | 12345, &query)) goto bad;
    if (mode == 3)
    {
        if (batch.moves[0].source_slot != 1 || batch.moves[0].destination_slot
            || batch.moves[0].flags != (RHH_PARTY_BEFORE_OCCUPIED | RHH_PARTY_AFTER_OCCUPIED)) goto bad;
        ++data[5]; puts("OWNED_PARTY_REORDER_OBSERVED"); return;
    }
    if (mode == 4 || mode == 5)
    {
        const struct RhhPartyRelocation *move = &batch.moves[0];
        unsigned source = mode == 4 ? 1 : 5, destination = mode == 4 ? 0 : 1, iv = mode == 4 ? 27 : 31;
        if (data[7] != (mode == 4 ? 1 : 3)
            || move->source_slot != source || move->destination_slot != destination || move->flags != 15
            || move->before.ivs[RHH_STAT_HP] != iv || move->after.ivs[RHH_STAT_HP] != iv) goto bad;
        ++data[5]; puts("OWNED_PARTY_REORDER_OBSERVED"); return;
    }
    if (mode == 6)
    {
        const struct RhhPartyRelocation *move = &batch.moves[0];
        if (data[8] != 1 || move->source_slot != 2 || move->destination_slot != 1 || move->flags != 15
            || move->before.ivs[RHH_STAT_HP] != 31 || move->after.ivs[RHH_STAT_HP] != 31) goto bad;
        ++data[5]; puts("OWNED_PARTY_REORDER_OBSERVED"); return;
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        const struct RhhPartyRelocation *move = &batch.moves[i];
        unsigned source = mode == 1 ? i : 2 + 2 * i, destination = mode == 1 ? 1 - i : i;
        unsigned iv = i ? 27 : 31;
        if (move->source_slot != source || move->destination_slot != destination || move->flags != 15
            || move->before.location != RHH_MON_PARTY || move->before.slot != source
            || move->after.location != RHH_MON_PARTY || move->after.slot != destination
            || move->before.identity != (((uint64_t)54321 << 32) | 12345)
            || move->after.identity != move->before.identity
            || move->before.ivs[RHH_STAT_HP] != iv || move->after.ivs[RHH_STAT_HP] != iv) goto bad;
    }
    ++data[5]; puts("OWNED_PARTY_REORDER_OBSERVED"); return;
bad:
    printf("OWNED_IDENTITY_BAD reorder mode=%u count=%u cause=%u moves=%u\n", mode, count, batch.cause, batch.count);
}
static void FusionTransferred(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned count = data[8];
    struct RhhFusionTransfer transfer = {.struct_size = sizeof(transfer)};
    struct RhhFusionTransfer small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    unsigned cause = count == 0 || count == 3 ? RHH_FUSION_STORE : RHH_FUSION_RESTORE;
    unsigned source = cause == RHH_FUSION_STORE ? RHH_MON_PARTY : RHH_MON_FUSION_STORAGE;
    unsigned destination = cause == RHH_FUSION_STORE ? RHH_MON_FUSION_STORAGE : count == 2 ? RHH_MON_BOX : RHH_MON_PARTY;
    unsigned flags = count == 3 ? 12 : 15;
    if (count > 3 || owned->fusion_transfer_context(&transfer) || !owned->fusion_transfer_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || transfer.run_id != 456 || transfer.sequence != count
        || event->ordinal != count || transfer.cause != cause || transfer.flags != flags || transfer.primary_slot
        || transfer.source_location != source || transfer.source_slot != (cause == RHH_FUSION_STORE ? 1u : 0u)
        || transfer.destination_location != destination || (count != 2 && transfer.destination_box)
        || (count == 0 && transfer.destination_slot) || (count == 1 && transfer.destination_slot != 2)
        || (count == 3 && transfer.destination_slot) || event->args[0] != cause || event->args[1] != source
        || event->args[2] != destination || event->args[3] != flags || !owned->identity_query(identity, &query)
        || transfer.primary_before.identity != identity || transfer.primary_after.identity != identity
        || transfer.primary_before.slot || transfer.primary_after.slot
        || transfer.primary_before.species == transfer.primary_after.species
        || (count == 0 && data[5] != 5) || (count == 1 && data[5] != 6)) goto bad;
    if (count == 3)
    {
        if (event->subject || event->event_key) goto bad;
    }
    else if (transfer.before.identity != identity || transfer.after.identity != identity
        || transfer.before.ivs[RHH_STAT_HP] != 27 || transfer.after.ivs[RHH_STAT_HP] != 27
        || transfer.before.location != source || transfer.after.location != destination
        || transfer.after.box != transfer.destination_box || transfer.after.slot != transfer.destination_slot
        || event->subject != identity || event->event_key != transfer.after.species
        || (count == 2 && (old->box_at(transfer.destination_box, transfer.destination_slot, &actual)
                           || actual.ivs[RHH_STAT_HP] != 27))) goto bad;
    ++data[8]; puts("OWNED_FUSION_TRANSFER_OBSERVED"); return;
bad:
    printf("OWNED_IDENTITY_BAD fusion count=%u cause=%u source=%u/%u destination=%u/%u/%u flags=%u\n",
           count, transfer.cause, transfer.source_location, transfer.source_slot, transfer.destination_location,
           transfer.destination_box, transfer.destination_slot, transfer.flags);
}
static void DaycareTransferred(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned count = data[7];
    struct RhhDaycareTransfer transfer = {.struct_size = sizeof(transfer)};
    struct RhhDaycareTransfer small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    unsigned cause = count < 2 || count == 4 ? RHH_DAYCARE_DEPOSIT : count == 2 ? RHH_DAYCARE_WITHDRAW : RHH_DAYCARE_SHIFT;
    unsigned source = count == 0 || count == 4 ? RHH_MON_PARTY : count == 1 ? RHH_MON_BOX : RHH_MON_DAYCARE;
    unsigned sourceBox = count == 1 ? 2 : 0;
    unsigned sourceSlot = count == 1 ? 3 : count == 3 ? 1 : 0;
    unsigned destination = count == 2 ? RHH_MON_PARTY : RHH_MON_DAYCARE;
    unsigned destinationSlot = count == 1 ? 1 : count == 2 ? 5 : 0;
    unsigned iv = count == 1 || count == 3 ? 27 : 31;
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    if (count > 4 || owned->daycare_transfer_context(&transfer) || !owned->daycare_transfer_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || transfer.run_id != 456 || transfer.sequence != count
        || event->ordinal != count || transfer.cause != cause || transfer.flags != (count == 4 ? 0u : 3u)
        || transfer.source_location != source || transfer.source_box != sourceBox || transfer.source_slot != sourceSlot
        || transfer.destination_location != destination || transfer.destination_box || transfer.destination_slot != destinationSlot
        || event->args[0] != cause || event->args[1] != source || event->args[2] != destination || event->args[3] != transfer.flags
        || !owned->identity_query(identity, &query)
        || (count == 0 && data[5] != 3) || ((count == 1 || count == 2) && data[5] != 4)
        || (count == 3 && data[5] != 5)) goto bad;
    if (count == 4)
    {
        if (event->subject || event->event_key) goto bad;
        ++data[7]; puts("OWNED_DAYCARE_TRANSFER_OBSERVED"); return;
    }
    if (transfer.before.location != source || transfer.before.box != sourceBox || transfer.before.slot != sourceSlot
        || transfer.after.location != destination || transfer.after.slot != destinationSlot
        || transfer.before.identity != identity || transfer.after.identity != identity
        || transfer.before.ivs[RHH_STAT_HP] != iv || transfer.after.ivs[RHH_STAT_HP] != iv
        || event->subject != identity || event->event_key != transfer.after.species) goto bad;
    ++data[7]; puts("OWNED_DAYCARE_TRANSFER_OBSERVED"); return;
bad:
    printf("OWNED_IDENTITY_BAD daycare count=%u cause=%u source=%u/%u/%u destination=%u/%u/%u flags=%u\n",
             count, transfer.cause, transfer.source_location, transfer.source_box, transfer.source_slot,
             transfer.destination_location, transfer.destination_box, transfer.destination_slot, transfer.flags);
}
static void Changed(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)event; (void)decision; unsigned char *data = state;
    struct RhhOwnedMonChange change = {.struct_size = sizeof(change)};
    if (owned->change_context(&change) || change.flags != RHH_OWNED_CHANGE_RESET || data[9])
        puts("OWNED_IDENTITY_BAD battle menu change");
    else { ++data[9]; puts("OWNED_BATTLE_BASELINE_OBSERVED"); }
}
static void CatchPlaced(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned count = data[10];
    struct RhhCatchPlacement placement = {.struct_size = sizeof(placement)};
    struct RhhCatchPlacement small = {.struct_size = 1, .cause = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    unsigned cause = count == 0 || count == 3 ? RHH_CATCH_DISPLACED_PARTY : RHH_CATCH_ACQUIRED;
    unsigned source = cause == RHH_CATCH_DISPLACED_PARTY ? RHH_MON_PARTY : RHH_MON_STAGED;
    unsigned destination = count == 1 ? RHH_MON_PARTY : RHH_MON_BOX;
    unsigned flags = count == 3 ? 0 : 3;
    uint64_t before = cause == RHH_CATCH_ACQUIRED ? ((uint64_t)777 << 32) | 33333 : ((uint64_t)54321 << 32) | 12345;
    uint64_t after = cause == RHH_CATCH_ACQUIRED ? ((uint64_t)54321 << 32) | 33333 : before;
    if (count > 3 || owned->catch_placement_context(&placement) || !owned->catch_placement_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || placement.run_id != 456 || placement.sequence != count
        || event->ordinal != count || placement.cause != cause || placement.flags != flags
        || placement.source_location != source || placement.source_slot != 1
        || placement.destination_location != destination
        || (count == 1 && (placement.destination_box || placement.destination_slot != 1))
        || event->args[0] != cause || event->args[1] != source || event->args[2] != destination
        || event->args[3] != flags || !owned->identity_query(before, &query)) goto bad;
    if (count == 3)
    {
        if (event->subject || event->event_key) goto bad;
    }
    else if (placement.before.identity != before || placement.after.identity != after
        || placement.before.location != source || placement.after.location != destination
        || placement.after.box != placement.destination_box || placement.after.slot != placement.destination_slot
        || event->subject != before || event->event_key != placement.after.species
        || (destination == RHH_MON_BOX && (old->box_at(placement.destination_box, placement.destination_slot, &actual)
            || actual.identity != after))) goto bad;
    ++data[10]; puts("OWNED_CATCH_PLACEMENT_OBSERVED"); return;
bad:
    printf("OWNED_IDENTITY_BAD catch count=%u cause=%u source=%u/%u dest=%u/%u/%u flags=%u\n",
           count, placement.cause, placement.source_location, placement.source_slot, placement.destination_location,
           placement.destination_box, placement.destination_slot, placement.flags);
}
static void IngameTrade(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned count = data[11];
    struct RhhIngameTrade trade = {.struct_size = sizeof(trade)};
    struct RhhIngameTrade small = {.struct_size = 1, .source_slot = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedMonSnapshot actual = {.struct_size = sizeof(actual)};
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    unsigned source = count == 1 ? RHH_MON_BOX : RHH_MON_PARTY;
    unsigned sourceBox = count == 1 ? 2 : 0, sourceSlot = count == 1 ? 5 : count == 2 ? 1 : 0;
    unsigned flags = count == 2 ? 6 : 7;
    unsigned sentIv = count == 1 ? 27 : 31, receivedIv = count == 0 ? 27 : 31;
    if (count > 2 || owned->ingame_trade_context(&trade) || !owned->ingame_trade_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || trade.run_id != 456 || trade.sequence != count
        || event->ordinal != count || trade.flags != flags
        || trade.source_location != source || trade.source_box != sourceBox || trade.source_slot != sourceSlot
        || trade.destination_location != source || trade.destination_box || trade.destination_slot != (count == 2 ? 1u : 0u)
        || trade.received_before.location != RHH_MON_STAGED || trade.received_before.identity != identity
        || trade.received_before.ivs[RHH_STAT_HP] != receivedIv || trade.received_after.identity != identity
        || trade.received_after.location != source || trade.received_after.ivs[RHH_STAT_HP] != receivedIv
        || event->args[0] != flags || event->args[1] != source || event->args[2] != source || event->args[3]
        || event->event_key != trade.received_after.species || !owned->identity_query(identity, &query)
        || (count == 2 && event->subject)
        || (count != 2 && (trade.sent.identity != identity || trade.sent.ivs[RHH_STAT_HP] != sentIv || event->subject != identity))
        || (count == 1 && (old->box_at(0, 0, &actual) || actual.ivs[RHH_STAT_HP] != receivedIv)))
    { printf("OWNED_IDENTITY_BAD trade count=%u source=%u/%u/%u destination=%u/%u/%u flags=%u\n",
             count, trade.source_location, trade.source_box, trade.source_slot,
             trade.destination_location, trade.destination_box, trade.destination_slot, trade.flags); return; }
    ++data[11]; puts("OWNED_INGAME_TRADE_OBSERVED");
}
static void LinkTrade(const struct RhhModEvent *event, const struct RhhModDecision *decision, void *state)
{
    (void)decision; unsigned char *data = state; unsigned count = data[12];
    struct RhhLinkTrade trade = {.struct_size = sizeof(trade)};
    struct RhhLinkTrade small = {.struct_size = 1, .party_slot = 999}, saved = small;
    struct RhhOwnedIdentityQuery query = {.struct_size = sizeof(query)};
    struct RhhOwnedMonSnapshot mon = {.struct_size = sizeof(mon), .identity = 999}, savedMon = mon;
    struct RhhSavedLinkTrade receipt = {.struct_size = sizeof(receipt)};
    struct RhhLinkHistoryCheckpoint checkpoint = {.struct_size = sizeof(checkpoint)};
    struct RhhLinkHistoryReconcile reconciliation = {.struct_size = sizeof(reconciliation)};
    uint64_t identity = ((uint64_t)54321 << 32) | 12345;
    unsigned transport = count ? RHH_LINK_TRADE_WIRELESS : RHH_LINK_TRADE_CABLE;
    unsigned sentIv = count ? 27 : 31, receivedIv = count ? 31 : 27;
    if (count > 1 || owned->link_trade_context(&trade) || !owned->link_trade_context(&small)
        || memcmp(&small, &saved, sizeof(small)) || trade.run_id != 456 || trade.sequence != count
        || event->ordinal != count || trade.flags != 7 || trade.party_slot || trade.transport != transport
        || trade.sent.identity != identity || trade.sent.location != RHH_MON_PARTY || trade.sent.slot
        || trade.sent.ivs[RHH_STAT_HP] != sentIv || trade.received_before.identity != identity
        || trade.received_before.location != RHH_MON_STAGED || trade.received_before.ivs[RHH_STAT_HP] != receivedIv
        || trade.received_after.identity != identity || trade.received_after.location != RHH_MON_PARTY
        || trade.received_after.slot || trade.received_after.ivs[RHH_STAT_HP] != receivedIv
        || (count == 1 && trade.received_after.species == trade.received_before.species)
        || event->subject != identity || event->event_key != trade.received_after.species
        || event->args[0] != 7 || event->args[1] || event->args[2] != transport || event->args[3]
        || owned->link_trade_receipt_context(&receipt) || receipt.serial != count + 1
        || receipt.trade.run_id != 456 || receipt.trade.flags != 7 || receipt.trade.transport != transport
        || receipt.trade.received_after.species != trade.received_after.species
        || owned->link_trade_history_count() != count + 1
        || owned->link_trade_checkpoint(count, &checkpoint) || checkpoint.run_id != 456 || checkpoint.serial != count + 1
        || owned->link_trade_reconcile(&checkpoint, &reconciliation) || reconciliation.status != RHH_LINK_HISTORY_UP_TO_DATE
        || owned->trade_state() != RHH_OWNED_TRADE_LINK_ACTIVE || !owned->identity_query(identity, &query)
        || !old->party_at(0, &mon) || memcmp(&mon, &savedMon, sizeof(mon)))
    { printf("OWNED_IDENTITY_BAD link trade count=%u flags=%u slot=%u transport=%u\n",
             count, trade.flags, trade.party_slot, trade.transport); return; }
    ++data[12]; puts("OWNED_LINK_TRADE_SAVED_OBSERVED");
}
static const struct RhhModHook hooks[] = {
    {sizeof(struct RhhModHook), RHH_MOD_UPDATE, RHH_OBSERVE, 0, NULL, Update},
    {sizeof(struct RhhModHook), RHH_OWNED_MON_RELEASED, RHH_OBSERVE, 0, NULL, Released},
    {sizeof(struct RhhModHook), RHH_OWNED_MON_MOVED, RHH_OBSERVE, 0, NULL, Moved},
    {sizeof(struct RhhModHook), RHH_OWNED_MON_PICKED_UP, RHH_OBSERVE, 0, NULL, PickedUp},
    {sizeof(struct RhhModHook), RHH_OWNED_STORAGE_UNREADABLE, RHH_OBSERVE, 0, NULL, StorageUnreadable},
    {sizeof(struct RhhModHook), RHH_OWNED_GIFT_RECEIVED, RHH_OBSERVE, 0, NULL, GiftReceived},
    {sizeof(struct RhhModHook), RHH_OWNED_DEBUG_CREATED, RHH_OBSERVE, 0, NULL, DebugCreated},
    {sizeof(struct RhhModHook), RHH_OWNED_MYSTERY_GIFT_RECEIVED, RHH_OBSERVE, 0, NULL, MysteryGift},
    {sizeof(struct RhhModHook), RHH_OWNED_EVOLUTION_COMMITTED, RHH_OBSERVE, 0, NULL, EvolutionCommitted},
    {sizeof(struct RhhModHook), RHH_PARTY_REORDERED, RHH_OBSERVE, 0, NULL, Reordered},
    {sizeof(struct RhhModHook), RHH_OWNED_DAYCARE_TRANSFERRED, RHH_OBSERVE, 0, NULL, DaycareTransferred},
    {sizeof(struct RhhModHook), RHH_OWNED_FUSION_TRANSFERRED, RHH_OBSERVE, 0, NULL, FusionTransferred},
    {sizeof(struct RhhModHook), RHH_OWNED_MON_CHANGED, RHH_OBSERVE, 0, NULL, Changed},
    {sizeof(struct RhhModHook), RHH_OWNED_CATCH_PLACED, RHH_OBSERVE, 0, NULL, CatchPlaced},
    {sizeof(struct RhhModHook), RHH_OWNED_INGAME_TRADE_COMPLETED, RHH_OBSERVE, 0, NULL, IngameTrade},
    {sizeof(struct RhhModHook), RHH_OWNED_LINK_TRADE_SAVED, RHH_OBSERVE, 0, NULL, LinkTrade}
};
static const struct RhhServiceRequirement requirements[] = {
    {sizeof(struct RhhServiceRequirement), RHH_SERVICE_VERSION(1, 42), RHH_SERVICE_REQUIRED, 0, "rhh:service/owned_mon"}
};
static int Merge(uint8_t *state, const uint8_t *journal, uint32_t size)
{
    if (!state || !journal || size != 13) return -1;
    if (state[12] < journal[12]) state[12] = journal[12];
    return 0;
}
static const struct RhhModDefinition definition = {.struct_size = sizeof(definition), .abi_version = RHH_MOD_ABI_SERVICES,
    .id = "test:owned_identity", .flags = RHH_MOD_DURABLE, .state_size = 13, .merge = Merge,
    .hook_count = 16, .hooks = hooks, .service_requirement_count = 1, .service_requirements = requirements};
#ifdef _WIN32
__declspec(dllexport)
#endif
const struct RhhModDefinition *RHH_ModEntry_v1(const struct RhhModHostV1 *host)
{
    if (!host || host->struct_size < sizeof(*host) || !host->get_service) return NULL;
    old = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 1));
    owned = (const void *)host->get_service("rhh:service/owned_mon", RHH_SERVICE_VERSION(1, 42));
    return old && owned && &old->header == &owned->header && old->header.struct_size >= RHH_OWNED_MON_SERVICE_V1_42_SIZE ? &definition : NULL;
}
