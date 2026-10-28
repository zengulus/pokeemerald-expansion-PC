// Exercise the production decoder's taken/skipped branches and return addresses.
#include <stdio.h>
#include <stdlib.h>
#include "../../src/contest_ai.c"

const u8 *gAIScriptPtr;
struct ContestResources *gContestResources;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "contest script: %s\n", #c); exit(1); } } while (0)

int main(void)
{
    struct Contest contest = {0};
    struct ContestAIInfo ai = {0};
    struct ContestResources resources = {.contest = &contest, .aiData = &ai};
    gContestResources = &resources;
    static const u8 target[] = {0};
    const u8 *pointer = target;
    u8 bytes[8 + sizeof(pointer)] = {0};
    bytes[1] = 1;
    memcpy(bytes + 2, &pointer, sizeof(pointer));
    gAIScriptPtr = bytes;
    contest.appealNumber = 0;
    ContestAICmd_if_appeal_num_less_than();
    CHECK(gAIScriptPtr == target);
    gAIScriptPtr = bytes;
    contest.appealNumber = 2;
    ContestAICmd_if_appeal_num_less_than();
    CHECK(gAIScriptPtr == bytes + 2 + sizeof(pointer));

    memset(bytes, 0, sizeof(bytes));
    bytes[2] = 17;
    memcpy(bytes + 4, &pointer, sizeof(pointer));
    ai.vars[0] = 17;
    gAIScriptPtr = bytes;
    ContestAICmd_if_eq();
    CHECK(gAIScriptPtr == target);
    ai.vars[0] = 18;
    gAIScriptPtr = bytes;
    ContestAICmd_if_eq();
    CHECK(gAIScriptPtr == bytes + 4 + sizeof(pointer));

    memset(bytes, 0, sizeof(bytes));
    memcpy(bytes + 1, &pointer, sizeof(pointer));
    gAIScriptPtr = bytes;
    ContestAICmd_call();
    CHECK(gAIScriptPtr == target && ai.stackSize == 1);
    ContestAICmd_end();
    CHECK(gAIScriptPtr == bytes + 1 + sizeof(pointer) && ai.stackSize == 0);
    puts("contest script branches and full-width call/return passed");
    return 0;
}
