/**
 * 任务管理
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "comm/cpu_instr.h"
#include "core/task.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "cpu/cpu.h"
#include "cpu/mmu.h"
#include "ipc/mutex.h"
#include "core/syscall.h"

static task_manager_t task_manager;               // 任务管理器
static uint32_t idle_task_stack[IDLE_STACK_SIZE]; // 空闲任务堆栈
static task_t task_tab[TASK_MAX_COUNT];
static mutex_t task_tab_mutex;

static int tss_init(task_t *task, uint32_t entry, uint32_t esp)
{
    // 为TSS分配GDT
    int tss_sel = gdt_alloc_desc();
    if (tss_sel < 0)
    {
        log_printf("alloc tss failed.\n");
        return -1;
    }

    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t),
                     SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);

    // tss段初始化
    kernel_memset(&task->tss, 0, sizeof(tss_t));
    task->tss.eip = entry;
    task->tss.esp = task->tss.esp0 = esp;
    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.eip = entry;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es = task->tss.ss = task->tss.ds = task->tss.fs = task->tss.gs =KERNEL_SELECTOR_DS; // 暂时写死
    task->tss.cs = KERNEL_SELECTOR_CS;                                                             // 暂时写死
    task->tss.iomap = 0;

    // 页表初始化
    uint32_t page_dir = memory_create_uvm();
    if (page_dir == 0)  // 错误资源回收
    {
        gdt_free_sel(tss_sel);
        return -1;
    }
    task->tss.cr3 = page_dir;

    task->tss_sel = tss_sel;
    return 0;
}

/**
 * @brief 初始化任务
 */
int task_init(task_t *task, const char *name, uint32_t entry, uint32_t esp)
{
    ASSERT(task != (task_t *)0);

    int err = tss_init(task, entry, esp);
    if (err < 0)
    {
        log_printf("init task failed.\n");
        return err;
    }

    // 任务字段初始化
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->sleep_ticks = 0;
    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->pid=(uint32_t)task; // 使用任务地址当作pid也不会重复
    task->parent=task_current(); // 当前进程就是其父进程
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    // 插入就绪队列中和所有的任务队列中
    irq_state_t state = irq_enter_protection(); // 使用关中断保护
    task_set_ready(task);
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}

void simple_switch(uint32_t **from, uint32_t *to);

/**
 * @brief 切换至指定任务
 */
void task_switch_from_to(task_t *from, task_t *to)
{
    switch_to_tss(to->tss_sel);
    // simple_switch(&from->stack, to->stack);
}

void first_task_entry(void);

/**
 * @brief 初始进程的初始化
 * 没有采用从磁盘加载的方式，因为需要用到文件系统，并且最好是和kernel绑在一定，这样好加载
 * 当然，也可以采用将init的源文件和kernel的一起编译。此里要调整好kernel.lds，在其中
 * 将init加载地址设置成和内核一起的，运行地址设置成用户进程运行的高处。
 * 不过，考虑到init可能用到newlib库，如果与kernel合并编译，在lds中很难控制将newlib的
 * 代码与init进程的放在一起，有可能与kernel放在了一起。
 * 综上，最好是分离。
 */
void task_first_init(void)
{
    extern uint8_t s_first_task[], e_first_task[];
    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task); // 获取实际代码大小
    uint32_t alloc_size = 16 * MEM_PAGE_SIZE;                     // 这里因为还有栈的原因需要多分配一点
    ASSERT(copy_size < alloc_size);

    uint32_t first_start = (uint32_t)first_task_entry; // 使用的是虚拟地址

    // 第一个任务代码量小一些，好和栈放在1个页面呢
    // 这样就不要立即考虑还要给栈分配空间的问题   first_start+alloc_size 把分配空间的末尾当作栈空间(下压栈)
    task_init(&task_manager.first_task, "first task", first_start, first_start+alloc_size); // 里面的值不必要写
    task_manager.curr_task = &task_manager.first_task;

    // 更新页表地址为自己的
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // 写TR寄存器，指示当前运行的第一个任务
    write_tr(task_manager.first_task.tss_sel);

    memory_alloc_for_page_dir(task_current()->tss.cr3, first_start, alloc_size, PTE_P | PTE_W); // 分配指定数量的内存
    kernel_memcpy((uint8_t *)first_start, s_first_task, copy_size);                             // 把对应的代码拷贝到对应区域 原代码所在区域采用一一映射，目标区域采用虚拟地址
}

/**
 * @brief 返回初始任务
 */
task_t *task_first_task(void)
{
    return &task_manager.first_task;
}

/**
 * @brief 空闲任务
 */
static void idle_task_entry(void)
{
    for (;;)
    {
        hlt();
    }
}

/**
 * @brief 任务管理器初始化
 */
void task_manager_init(void)
{
    kernel_memset(task_tab,0, sizeof(task_tab));
    mutex_init(&task_tab_mutex);
    task_manager.app_data_sel = gdt_alloc_desc();
    segment_desc_set(task_manager.app_data_sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_CPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = gdt_alloc_desc();
    segment_desc_set(task_manager.app_code_sel, 0x00000000, 0xFFFFFFFF,
                     SEG_P_PRESENT | SEG_CPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);

    // 各队列初始化
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);

    // 空闲任务初始化
    task_init(&task_manager.idle_task,
              "idle task",
              (uint32_t)idle_task_entry,
              (uint32_t)(idle_task_stack + IDLE_STACK_SIZE)); // 里面的值不必要写

    task_manager.curr_task = (task_t *)0;
}

/**
 * @brief 将任务插入就绪队列
 */
void task_set_ready(task_t *task)
{
    if (task != &task_manager.idle_task)
    {
        list_insert_last(&task_manager.ready_list, &task->run_node);
        task->state = TASK_READY;
    }
}

/**
 * @brief 将任务从就绪队列移除
 */
void task_set_block(task_t *task)
{
    if (task != &task_manager.idle_task)
    {
        list_remove(&task_manager.ready_list, &task->run_node);
    }
}
/**
 * @brief 获取下一将要运行的任务
 */
static task_t *task_next_run(void)
{
    // 如果没有任务，则运行空闲任务
    if (list_count(&task_manager.ready_list) == 0)
    {
        return &task_manager.idle_task;
    }

    // 普通任务
    list_node_t *task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node, task_t, run_node);
}

/**
 * @brief 将任务加入睡眠状态
 */
void task_set_sleep(task_t *task, uint32_t ticks)
{
    if (ticks <= 0)
    {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief 将任务从延时队列移除
 *
 * @param task
 */
void task_set_wakeup(task_t *task)
{
    list_remove(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief 获取当前正在运行的任务
 */
task_t *task_current(void)
{
    return task_manager.curr_task;
}

/**
 * @brief 当前任务主动放弃CPU
 */
int sys_yield(void)
{
    irq_state_t state = irq_enter_protection();

    if (list_count(&task_manager.ready_list) > 1)
    {
        task_t *curr_task = task_current();

        // 如果队列中还有其它任务，则将当前任务移入到队列尾部
        task_set_block(curr_task);
        task_set_ready(curr_task);

        // 切换至下一个任务，在切换完成前要保护，不然可能下一任务
        // 由于某些原因运行后阻塞或删除，再回到这里切换将发生问题
        task_dispatch();
    }
    irq_leave_protection(state);

    return 0;
}

/**
 * @brief 进行一次任务调度
 */
void task_dispatch(void)
{
    task_t *to = task_next_run();
    if (to != task_manager.curr_task)
    {
        task_t *from = task_manager.curr_task;
        task_manager.curr_task = to;

        to->state = TASK_RUNNING;
        task_switch_from_to(from, to);
    }
}

/**
 * @brief 时间处理
 * 该函数在中断处理函数中调用
 */
void task_time_tick(void)
{
    task_t *curr_task = task_current();

    // 时间片的处理
    irq_state_t state = irq_enter_protection();
    if (--curr_task->slice_ticks == 0)
    {
        // 时间片用完，重新加载时间片
        // 对于空闲任务，此处减未用
        curr_task->slice_ticks = curr_task->time_slice;

        // 调整队列的位置到尾部，不用直接操作队列
        task_set_block(curr_task);
        task_set_ready(curr_task);
    }

    // 睡眠处理
    list_node_t *curr = list_first(&task_manager.sleep_list);
    while (curr)
    {
        list_node_t *next = list_node_next(curr);

        task_t *task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0)
        {
            // 延时时间到达，从睡眠队列中移除，送至就绪队列
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }

    task_dispatch();
    irq_leave_protection(state);
}

/**
 * @brief 任务进入睡眠状态
 *
 * @param ms
 */
void sys_sleep(uint32_t ms)
{
    // 至少延时1个tick
    if (ms < OS_TICK_MS)
    {
        ms = OS_TICK_MS;
    }

    irq_state_t state = irq_enter_protection();

    // 从就绪队列移除，加入睡眠队列
    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + (OS_TICK_MS - 1)) / OS_TICK_MS);

    // 进行一次调度
    task_dispatch();

    irq_leave_protection(state);
}

uint32_t sys_getpid (){
    return task_current()->pid;
}

static task_t *alloc_task(){
    mutex_lock(&task_tab_mutex);

    for (int i = 0; i < TASK_MAX_COUNT; ++i) {
        task_t *task= task_tab+i;
        if(task->pid==0){// pid为0的视为没有被使用的
            return task;
        }
    }

    mutex_unlock(&task_tab_mutex);
    return 0;
}

static void free_task(task_t *task){
    mutex_lock(&task_tab_mutex);

    task->pid=0;

    mutex_unlock(&task_tab_mutex);
}

void new_task(){
    for (;;){
        hlt();
    }
}

uint32_t sys_fork(syscall_frame_t *frame){
//    task_t *parent = task_current();
//
//    task_t *task = alloc_task();// 创建初始化任务
//    task_init(task,parent->name,frame->eip,frame->esp); // 直接复用栈空间？
//
//    // 父子进程需要在相同的上下文中执行
//    tss_t *tss = &task->tss;
//    tss->eax=0; // 最终会把返回值设置进eax 设置其默认值为0 父进程走完整个流程可以获取到pid并赋值给eax 子进程是设置到调用处的，eax没有赋值为默认值0
//    tss->ebx=frame->ebx;
//    tss->ecx=frame->ecx;
//    tss->edx=frame->edx;
//    tss->esi=frame->esi;
//    tss->edi=frame->edi;
//    tss->ebp=frame->ebp;
//
//    tss->cs=frame->cs;
//    tss->ds=frame->ds;
//    tss->es=frame->es;
//    tss->fs=frame->fs;
//    tss->gs=frame->gs;
//    tss->eflags=frame->eflags;
//
//    tss.cr3= memory_copy_uvm(parent->tss.cr3);

    task_t *task = alloc_task();// 创建初始化任务
    uint32_t addr= memory_alloc_page();
    task_init(task,"TEST",(uint32_t)new_task,addr+MEM_PAGE_SIZE); // 直接复用栈空间？
    return task->pid;
}
