#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "task.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "task: %s\n", #c); exit(1); } } while (0)
static unsigned sCalls;
static void Followup(u8 id) { CHECK(gTasks[id].data[0] == 100); ++sCalls; }
static void First(u8 id) { SwitchTaskToFollowupFunc(id); }

int main(void)
{
    unsigned object = 123;
    ResetTasks();
    u8 id = CreateTask(First, 0);
    gTasks[id].data[0] = 100;
    SetWordTaskArg(id, 2, 0xfedcba98);
    CHECK(GetWordTaskArg(id, 2) == 0xfedcba98);
    SetTaskPointerArg(id, 8, &object);
    CHECK(GetTaskPointerArg(id, 8) == &object);
    CHECK(*(unsigned *)GetTaskPointerArg(id, 8) == 123);
    CHECK(gTasks[id].data[0] == 100 && sizeof(gTasks[id].data[0]) == 2);
#if UINTPTR_MAX > UINT32_MAX
    CHECK((uintptr_t)&object > UINT32_MAX);
#endif
    SetTaskFuncWithFollowupFunc(id, First, Followup);
    RunTasks();
    CHECK(sCalls == 0);
    RunTasks();
    CHECK(sCalls == 1);
    DestroyTask(id);
    CHECK(GetTaskPointerArg(id, 8) == NULL);
    CHECK(CreateTask(First, 0) == id);
    CHECK(GetTaskPointerArg(id, 8) == NULL && gTasks[id].data[0] == 0);
    puts("task scalar, full-width pointer, callback and lifecycle checks passed");
    return 0;
}
