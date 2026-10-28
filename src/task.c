#include "global.h"
#include "task.h"

COMMON_DATA struct Task gTasks[NUM_TASKS] = {0};

static void InsertTask(u8 newTaskId);
static u8 FindFirstActiveTask(void);

void ResetTasks(void)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
    {
        gTasks[i].isActive = FALSE;
        gTasks[i].func = TaskDummy;
        gTasks[i].prev = i;
        gTasks[i].next = i + 1;
        gTasks[i].priority = -1;
        memset(gTasks[i].data, 0, sizeof(gTasks[i].data));
#ifdef NATIVE
        memset(gTasks[i].pointerArgs, 0, sizeof(gTasks[i].pointerArgs));
        gTasks[i].followupFunc = NULL;
#endif
    }

    gTasks[0].prev = HEAD_SENTINEL;
    gTasks[NUM_TASKS - 1].next = TAIL_SENTINEL;
}

u8 CreateTask(TaskFunc func, u8 priority)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
    {
        if (!gTasks[i].isActive)
        {
            gTasks[i].func = func;
            gTasks[i].priority = priority;
            InsertTask(i);
            memset(gTasks[i].data, 0, sizeof(gTasks[i].data));
#ifdef NATIVE
            memset(gTasks[i].pointerArgs, 0, sizeof(gTasks[i].pointerArgs));
            gTasks[i].followupFunc = NULL;
#endif
            gTasks[i].isActive = TRUE;
            return i;
        }
    }

    return 0;
}

static void InsertTask(u8 newTaskId)
{
    u8 taskId = FindFirstActiveTask();

    if (taskId == NUM_TASKS)
    {
        // The new task is the only task.
        gTasks[newTaskId].prev = HEAD_SENTINEL;
        gTasks[newTaskId].next = TAIL_SENTINEL;
        return;
    }

    while (1)
    {
        if (gTasks[newTaskId].priority < gTasks[taskId].priority)
        {
            // We've found a task with a higher priority value,
            // so we insert the new task before it.
            gTasks[newTaskId].prev = gTasks[taskId].prev;
            gTasks[newTaskId].next = taskId;
            if (gTasks[taskId].prev != HEAD_SENTINEL)
                gTasks[gTasks[taskId].prev].next = newTaskId;
            gTasks[taskId].prev = newTaskId;
            return;
        }
        if (gTasks[taskId].next == TAIL_SENTINEL)
        {
            // We've reached the end.
            gTasks[newTaskId].prev = taskId;
            gTasks[newTaskId].next = gTasks[taskId].next;
            gTasks[taskId].next = newTaskId;
            return;
        }
        taskId = gTasks[taskId].next;
    }
}

void DestroyTask(u8 taskId)
{
    if (gTasks[taskId].isActive)
    {
        gTasks[taskId].isActive = FALSE;
#ifdef NATIVE
        memset(gTasks[taskId].pointerArgs, 0, sizeof(gTasks[taskId].pointerArgs));
        gTasks[taskId].followupFunc = NULL;
#endif

        if (gTasks[taskId].prev == HEAD_SENTINEL)
        {
            if (gTasks[taskId].next != TAIL_SENTINEL)
                gTasks[gTasks[taskId].next].prev = HEAD_SENTINEL;
        }
        else
        {
            if (gTasks[taskId].next == TAIL_SENTINEL)
            {
                gTasks[gTasks[taskId].prev].next = TAIL_SENTINEL;
            }
            else
            {
                gTasks[gTasks[taskId].prev].next = gTasks[taskId].next;
                gTasks[gTasks[taskId].next].prev = gTasks[taskId].prev;
            }
        }
    }
}

void RunTasks(void)
{
    u8 taskId = FindFirstActiveTask();

    if (taskId != NUM_TASKS)
    {
        do
        {
            gTasks[taskId].func(taskId);
            taskId = gTasks[taskId].next;
        } while (taskId != TAIL_SENTINEL);
    }
}

static u8 FindFirstActiveTask(void)
{
    u8 taskId;

    for (taskId = 0; taskId < NUM_TASKS; taskId++)
        if (gTasks[taskId].isActive == TRUE && gTasks[taskId].prev == HEAD_SENTINEL)
            break;

    return taskId;
}

void TaskDummy(u8 taskId)
{
}

void SetTaskFuncWithFollowupFunc(u8 taskId, TaskFunc func, TaskFunc followupFunc)
{
#ifdef NATIVE
    gTasks[taskId].followupFunc = followupFunc;
#if UINTPTR_MAX == UINT32_MAX
    SetWordTaskArg(taskId, NUM_TASK_DATA - 2, (uintptr_t)followupFunc);
#endif
#else
    u8 followupFuncIndex = NUM_TASK_DATA - 2; // Should be const.

    gTasks[taskId].data[followupFuncIndex] = (s16)((u32)followupFunc);
    gTasks[taskId].data[followupFuncIndex + 1] = (s16)((u32)followupFunc >> 16); // Store followupFunc as two half-words in the data array.
#endif
    gTasks[taskId].func = func;
}

void SwitchTaskToFollowupFunc(u8 taskId)
{
#ifdef NATIVE
    gTasks[taskId].func = gTasks[taskId].followupFunc;
#else
    u8 followupFuncIndex = NUM_TASK_DATA - 2; // Should be const.

    gTasks[taskId].func = (TaskFunc)((u16)(gTasks[taskId].data[followupFuncIndex]) | (gTasks[taskId].data[followupFuncIndex + 1] << 16));
#endif
}

bool8 FuncIsActiveTask(TaskFunc func)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; i++)
        if (gTasks[i].isActive == TRUE && gTasks[i].func == func)
            return TRUE;

    return FALSE;
}

u8 FindTaskIdByFunc(TaskFunc func)
{
    s32 i;

    for (i = 0; i < NUM_TASKS; i++)
        if (gTasks[i].isActive == TRUE && gTasks[i].func == func)
            return (u8)i;

    return TASK_NONE; // No task was found.
}

void SetWordTaskArg(u8 taskId, u8 dataElem, u32 value)
{
    if (dataElem < NUM_TASK_DATA - 1)
    {
        gTasks[taskId].data[dataElem] = value;
        gTasks[taskId].data[dataElem + 1] = value >> 16;
    }
}

u32 GetWordTaskArg(u8 taskId, u8 dataElem)
{
    if (dataElem < NUM_TASK_DATA - 1)
        return (u16)gTasks[taskId].data[dataElem] | ((u32)(u16)gTasks[taskId].data[dataElem + 1] << 16);
    else
        return 0;
}

void SetTaskPointerArg(u8 taskId, u8 slot, const void *pointer)
{
#ifdef NATIVE
    if (taskId < NUM_TASKS && slot < NUM_TASK_DATA)
    {
        gTasks[taskId].pointerArgs[slot] = (void *)pointer;
#if UINTPTR_MAX == UINT32_MAX
        // Retain the legacy halfwords while remaining manual readers migrate.
        SetWordTaskArg(taskId, slot, (uintptr_t)pointer);
#endif
    }
#else
    SetWordTaskArg(taskId, slot, (uintptr_t)pointer);
#endif
}

void *GetTaskPointerArg(u8 taskId, u8 slot)
{
#ifdef NATIVE
    return taskId < NUM_TASKS && slot < NUM_TASK_DATA ? gTasks[taskId].pointerArgs[slot] : NULL;
#else
    return (void *)(uintptr_t)GetWordTaskArg(taskId, slot);
#endif
}
