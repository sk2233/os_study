/**
 * 内存管理
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "tools/klib.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"
#include "cpu/mmu.h"

static addr_alloc_t paddr_alloc;        // 物理地址分配结构
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // 内核页目录表

/**
 * @brief 初始化地址分配结构
 * 以下不检查start和size的页边界，由上层调用者检查
 */
static void addr_alloc_init (addr_alloc_t * alloc, uint8_t * bits,
                    uint32_t start, uint32_t size, uint32_t page_size) {
    mutex_init(&alloc->mutex);
    alloc->start = start;
    alloc->size = size;
    alloc->page_size = page_size;
    bitmap_init(&alloc->bitmap, bits, alloc->size / page_size, 0);
}

/**
 * @brief 分配多页内存
 */
static uint32_t addr_alloc_page (addr_alloc_t * alloc, int page_count) {
    uint32_t addr = 0;

    mutex_lock(&alloc->mutex);

    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if (page_index >= 0) {
        addr = alloc->start + page_index * alloc->page_size;
    }

    mutex_unlock(&alloc->mutex);
    return addr;
}

/**
 * @brief 释放多页内存
 */
static void addr_free_page (addr_alloc_t * alloc, uint32_t addr, int page_count) {
    mutex_lock(&alloc->mutex);

    uint32_t pg_idx = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, pg_idx, page_count, 0);

    mutex_unlock(&alloc->mutex);
}

static void show_mem_info (boot_info_t * boot_info) {
    log_printf("mem region:");
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        log_printf("[%d]: 0x%x - 0x%x", i,
                    boot_info->ram_region_cfg[i].start,
                    boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

/**
 * @brief 获取可用的物理内存大小
 */
static uint32_t total_mem_size(boot_info_t * boot_info) {
    int mem_size = 0;

    // 简单起见，暂不考虑中间有空洞的情况
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}

pte_t * find_pte (pde_t * page_dir, uint32_t vaddr, int alloc) {
    pte_t * page_table;

    pde_t *pde = page_dir + pde_index(vaddr);
    if (pde->present) {
        page_table = (pte_t *)pde_paddr(pde);
    } else {
        // 如果不存在，则考虑分配一个
        if (alloc == 0) {
            return (pte_t *)0;
        }

        // 分配一个物理页表
        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);
        if (pg_paddr == 0) {
            return (pte_t *)0;
        }

        // 设置为用户可读写，将被pte中设置所覆盖  这里可以随便放宽条件
        pde->v = pg_paddr | PTE_P | PTE_W;

        // 为物理页表绑定虚拟地址的映射，这样下面就可以计算出虚拟地址了
        //kernel_pg_last[pde_index(vaddr)].v = pg_paddr | PTE_P | PTE_W;

        // 清空页表，防止出现异常
        // 这里虚拟地址和物理地址一一映射，所以直接写入
        page_table = (pte_t *)(pg_paddr);
        kernel_memset(page_table, 0, MEM_PAGE_SIZE);
    }

    return page_table + pte_index(vaddr);
}

/**
 * @brief 将指定的地址空间进行一页的映射
 */
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm) {
    for (int i = 0; i < count; i++) {
        // log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);

        pte_t * pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0) {
            // log_printf("create pte failed. pte == 0");
            return -1;
        }

        // 创建映射的时候，这条pte应当是不存在的。
        // 如果存在，说明可能有问题
        // log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);

        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief 根据内存映射表，构造内核页表
 */
void create_kernel_table (void) {
    extern uint8_t s_text[], e_text[], s_data[], e_data[];
    extern uint8_t kernel_base[];

    // 地址映射表, 用于建立内核级的地址映射
    // 地址不变，但是添加了属性
    static memory_map_t kernel_map[] = {
             {0,   0x80000,         0,              PTE_W}, // 内核区域一一映射
//       {kernel_base,   s_text,         0,              PTE_W},         // 内核栈区
//       {s_text,        e_text,         s_text,         0},         // 内核代码区
//       {s_data,        (void *)(MEM_EBDA_START - 1),   s_data,        PTE_W},      // 内核数据区

        // 扩展存储空间一一映射，方便直接操作
        {MEM_EXT_START, MEM_EXT_END,     MEM_EXT_START, PTE_W},
    };

    // 清空页目录表
    kernel_memset(kernel_page_dir, 0, sizeof(kernel_page_dir));

    // 清空后，然后依次根据映射关系创建映射表
    for (int i = 0; i < 2; i++) {
        memory_map_t * map = kernel_map + i;

        // 可能有多个页，建立多个页的配置
        // 简化起见，不考虑4M的情况
        // int vstart = down2(map->vstart, MEM_PAGE_SIZE);
        // int vend = up2(map->vend, MEM_PAGE_SIZE);
        int page_count = (map->vend - map->vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, map->vstart, map->pstart, page_count, map->perm);
    }
}

/**
 * @brief 创建进程的初始页表
 * 主要的工作创建页目录表，然后从内核页表中复制一部分
 */
uint32_t memory_create_uvm (void) {
    pde_t * page_dir = (pde_t *)addr_alloc_page(&paddr_alloc, 1);
    if (page_dir == 0) {
        return 0;
    }
    kernel_memset((void *)page_dir, 0, MEM_PAGE_SIZE);

    // 复制整个内核空间的页目录项，以便与其它进程共享内核空间
    // 用户空间的内存映射暂不处理，等加载程序时创建
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    // 内核空间在2G下，任务空间在2G上
    for (int i = 0; i < user_pde_start; i++) {
        // 复用操作系统的页表
        page_dir[i].v = kernel_page_dir[i].v;
    }

    return (uint32_t)page_dir;
}

uint32_t memory_copy_uvm (uint32_t page_dir){
    uint32_t res = memory_create_uvm();

    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t *pde = (pde_t *)(page_dir+user_pde_start);
    for (uint32_t i = user_pde_start; i < PDE_CNT; ++i) {
        if(!pde->present){
            continue;
        }
        pte_t *pte = (pte_t *)pde_paddr(pde);
        for (uint32_t j = 0; j < PTE_CNT; ++j) {
            if(!pte->present){
                continue;
            }
            uint32_t page = memory_alloc_page(); // 分配全新的页内存为物理内存
            // 虚拟地址的组合规则
            uint32_t vaddr =(i<<22)|(j<<12);
            // 页目录下的页表会在内部自动分配内存
            memory_create_map((pde_t *)res,vaddr,page,1, get_pte_perm(pte));
            kernel_memcpy((void *)page,(void *)vaddr,MEM_PAGE_SIZE);
        }
    }

    return res;
}

void memory_free_uvm (uint32_t page_dir){
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t *pde = (pde_t *)(page_dir+user_pde_start);
    for (uint32_t i = user_pde_start; i < PDE_CNT; ++i) {
        if(!pde->present){
            continue;
        }
        pte_t *pte = (pte_t *)pde_paddr(pde);
        for (uint32_t j = 0; j < PTE_CNT; ++j) {
            if(!pte->present){
                continue;
            }
            memory_free_page((uint32_t)pte);
        }
        memory_free_page((uint32_t)pde);
    }
    memory_free_page(page_dir);
}

/**
 * @brief 初始化内存管理系统
 * 该函数的主要任务：
 * 1、初始化物理内存分配器：将所有物理内存管理起来. 在1MB内存中分配物理位图
 * 2、重新创建内核页表：原loader中创建的页表已经不再合适
 */
void memory_init (boot_info_t * boot_info) {
    // 1MB内存空间起始，在链接脚本中定义
    extern uint8_t * mem_free_start;

    log_printf("mem init.");
    show_mem_info(boot_info);

    // 在内核数据后面放物理页位图
    uint8_t * mem_free = (uint8_t *)&mem_free_start;   // 2022年-10-1 经同学反馈，发现这里有点bug，改了下

    // 计算1MB以上空间的空闲内存容量，并对齐的页边界
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);   // 对齐到4KB页
    log_printf("Free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);

    // 4GB大小需要总共4*1024*1024*1024/4096/8=128KB的位图, 使用低1MB的RAM空间中足够
    // 该部分的内存仅跟在mem_free_start开始放置
    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, boot_info->ram_region_cfg[1].size, MEM_PAGE_SIZE);
    mem_free += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);

    // 到这里，mem_free应该比EBDA地址要小
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    // 创建内核页表并切换过去
    create_kernel_table();

    // 先切换到当前页表
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}

// 根据虚拟地址进行分配内存
int memory_alloc_for_page_dir(uint32_t page_dir,uint32_t vaddr,uint32_t size,int perm){
    int page_count = up2(size,MEM_PAGE_SIZE)/MEM_PAGE_SIZE;
    for (int i = 0; i < page_count; ++i) {
        uint32_t addr=addr_alloc_page(&paddr_alloc,1);// 物理内存可以不连续
        if(addr==0){
            log_printf("mem alloc failed no mem");
            return -1;
        }// 创建页表映射
        int  err = memory_create_map((pde_t *)page_dir,vaddr,addr,1,perm);
        if(err<0){
            log_printf("memory_create_map err = %d",err);
            return -1;
        }
        vaddr+=MEM_PAGE_SIZE; // 继续分配下一个
    }
    return 0;
}

uint32_t memory_alloc_page(){ // 返回的物理地址与逻辑地址一样，初始化时有这么一个映射
    return addr_alloc_page(&paddr_alloc,1);
}

pde_t *current_page_dir(){
    return (pde_t *)task_current()->tss.cr3;
}

void memory_free_page(uint32_t addr){
    if(addr<MEMORY_TASK_BASE){// 一一映射区域进行内存回收
        addr_free_page(&paddr_alloc,addr,1);
    } else{
        pte_t *pt=find_pte(current_page_dir(),addr,0);
        if(pt!=0){
            // pte_paddr 寻找对应的真实地址 进行内存清理
            addr_free_page(&paddr_alloc,pte_paddr(pt),1);
            pt->v=0;
        }
    }
}