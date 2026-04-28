/*
 * function_pointer_callback_demo.c
 *
 * 这是一个独立演示文件，用最小示例解释：
 * 1) 结构体里放函数指针
 * 2) 先注册（赋值）回调
 * 3) 事件发生时，通过函数指针调用回调
 *
 * 这个文件默认不参与你当前工程构建，只用于学习。
 */

#include <stdint.h>
#include <stdio.h>

/* 前向声明：先告诉编译器有这个结构体类型 */
typedef struct DemoDMAHandle DemoDMAHandle;

/* 回调函数类型：参数是 DemoDMAHandle*，返回 void */
typedef void (*DemoCallback)(DemoDMAHandle *hdma);

/* 模拟 DMA 句柄 */
struct DemoDMAHandle
{
    uint16_t buffer[16];            /* 模拟DMA写入的内存缓冲区 */
    volatile uint32_t halfCount;    /* 半传输事件计数 */
    volatile uint32_t fullCount;    /* 全传输事件计数 */

    /* 这两个成员就是“回调函数指针” */
    DemoCallback XferHalfCpltCallback;
    DemoCallback XferCpltCallback;
};

/* ---------- 用户自定义回调 ---------- */
static void UserHalfCallback(DemoDMAHandle *hdma)
{
    hdma->halfCount++;
    printf("[CB] half complete, halfCount=%lu\n", (unsigned long)hdma->halfCount);
}

static void UserFullCallback(DemoDMAHandle *hdma)
{
    hdma->fullCount++;
    printf("[CB] full complete, fullCount=%lu\n", (unsigned long)hdma->fullCount);
}

/* ---------- 注册回调（本质是函数指针赋值） ---------- */
static void RegisterCallbacks(DemoDMAHandle *hdma)
{
    hdma->XferHalfCpltCallback = UserHalfCallback;
    hdma->XferCpltCallback = UserFullCallback;
}

/* ---------- 模拟DMA硬件搬运数据 ---------- */
static void SimulateDmaWrite(DemoDMAHandle *hdma)
{
    uint16_t i;

    /* 假设DMA把外设数据搬到 buffer */
    for (i = 0; i < 8; i++)
    {
        hdma->buffer[i] = (uint16_t)(100 + i);  /* 前半区 */
    }

    /* 触发“半传输完成事件”：调用回调函数指针 */
    if (hdma->XferHalfCpltCallback != 0)
    {
        hdma->XferHalfCpltCallback(hdma);
    }

    for (i = 8; i < 16; i++)
    {
        hdma->buffer[i] = (uint16_t)(100 + i);  /* 后半区 */
    }

    /* 触发“全传输完成事件”：调用回调函数指针 */
    if (hdma->XferCpltCallback != 0)
    {
        hdma->XferCpltCallback(hdma);
    }
}

/* ---------- 对外演示入口 ---------- */
void RunFunctionPointerCallbackDemo(void)
{
    DemoDMAHandle hdma = {0};

    RegisterCallbacks(&hdma);
    SimulateDmaWrite(&hdma);

    /* 观察数据已经在缓冲区 */
    printf("buffer[0]=%u, buffer[7]=%u, buffer[8]=%u, buffer[15]=%u\n",
           hdma.buffer[0], hdma.buffer[7], hdma.buffer[8], hdma.buffer[15]);
}
