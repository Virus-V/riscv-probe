#include "femto.h"

#ifndef __riscv
#error not risc-v!!
#endif

#include "arch/riscv/csr.h"
#include "arch/riscv/trap.h"
#include "arch/riscv/encoding.h"
#include "arch/riscv/machine.h"

/* 页面大小 4KB */
#define PAGESIZE (1 << 12)
/* LEVELS */
#define LEVELS (3)

/* 虚拟地址 */
union vaddr {
  struct {
    uint64_t page_offset : 12;
    uint64_t vpn_0 : 9;
    uint64_t vpn_1 : 9;
    uint64_t vpn_2 : 9;
  } bits;
  uint64_t data;
};

/* 物理地址 */
union paddr {
  struct {
    uint64_t page_offset : 12;
    uint64_t ppn_0 : 9;
    uint64_t ppn_1 : 9;
    uint64_t ppn_2 : 26;
  } bits;
  uint64_t data;
};

/* PTE 页表项 */
union pte {
  struct {
    uint64_t V : 1;
    uint64_t R : 1;
    uint64_t W : 1;
    uint64_t X : 1;
    uint64_t U : 1;
    uint64_t G : 1;
    uint64_t A : 1;
    uint64_t D : 1;
    uint64_t RSW : 2;
    uint64_t ppn_0 : 9;
    uint64_t ppn_1 : 9;
    uint64_t ppn_2 : 26;
  } bits;
  uint64_t data;
};

/* Sv39是3级页表 */
uint64_t pte0[1 << 9] __attribute__((aligned(4096)));
uint64_t pte1[1 << 9] __attribute__((aligned(4096)));
uint64_t pte2[1 << 9] __attribute__((aligned(4096)));

uint32_t global_test_data = 100;

static void page_fault_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc) {

}

/* 增加页映射 */
void pmap_add(uintptr_t va, uintptr_t pa, size_t length, uint32_t prot) {
  union paddr phy_addr_u;
  union vaddr va_addr_u;
  union pte ptable_entry_u;
  uintptr_t va_addr, pa_addr;

  for (va_addr = va, pa_addr = pa; va_addr < va+length; va_addr+=PAGESIZE, pa_addr+=PAGESIZE) {
    va_addr_u.data = va_addr;

    /* 设置PTE0的页表项 */
    phy_addr_u.data = (uintptr_t)pte1;

    ptable_entry_u.data = 0;
    ptable_entry_u.bits.ppn_2 = phy_addr_u.bits.ppn_2;
    ptable_entry_u.bits.ppn_1 = phy_addr_u.bits.ppn_1;
    ptable_entry_u.bits.ppn_0 = phy_addr_u.bits.ppn_0;

    ptable_entry_u.bits.V = 1;
    ptable_entry_u.bits.R = 0;
    ptable_entry_u.bits.W = 0;
    ptable_entry_u.bits.X = 0;
    ptable_entry_u.bits.U = 0;

    ptable_entry_u.bits.A = 1;
    ptable_entry_u.bits.D = 1;

    pte0[va_addr_u.bits.vpn_2] = ptable_entry_u.data;

    /* 设置PTA1页表项 */
    phy_addr_u.data = (uintptr_t)pte2;

    ptable_entry_u.data = 0;
    ptable_entry_u.bits.ppn_2 = phy_addr_u.bits.ppn_2;
    ptable_entry_u.bits.ppn_1 = phy_addr_u.bits.ppn_1;
    ptable_entry_u.bits.ppn_0 = phy_addr_u.bits.ppn_0;

    ptable_entry_u.bits.V = 1;
    ptable_entry_u.bits.R = 0;
    ptable_entry_u.bits.W = 0;
    ptable_entry_u.bits.X = 0;
    ptable_entry_u.bits.U = 0;

    ptable_entry_u.bits.A = 1;
    ptable_entry_u.bits.D = 1;

    pte1[va_addr_u.bits.vpn_1] = ptable_entry_u.data;

    /* 设置PTA2页表项 */
    phy_addr_u.data = pa_addr;

    ptable_entry_u.data = 0;
    ptable_entry_u.bits.ppn_2 = phy_addr_u.bits.ppn_2;
    ptable_entry_u.bits.ppn_1 = phy_addr_u.bits.ppn_1;
    ptable_entry_u.bits.ppn_0 = phy_addr_u.bits.ppn_0;

    ptable_entry_u.bits.V = 1;
    ptable_entry_u.bits.R = 1;
    ptable_entry_u.bits.W = 1;
    ptable_entry_u.bits.X = 1;
    ptable_entry_u.bits.U = 0;

    ptable_entry_u.bits.A = 1;
    ptable_entry_u.bits.D = 1;

    pte2[va_addr_u.bits.vpn_0] = ptable_entry_u.data;

    printf("va => pa: %p => %p\n", va_addr, pa_addr);
  }
}

int main(int argc, char **argv)
{
  /* 设置异常处理函数 */
  set_trap_fn(page_fault_trap);

  uint32_t *global_test_data_va;

	printf("mmu table size is :%x\n", sizeof(pte0) + sizeof(pte1) + sizeof(pte2));
	printf("global_test_data: %p\n", &global_test_data);

  /* 直接映射 */
  pmap_add(0x80200000u, 0x80200000u, 2*1024*1024, 0);

  /* 将0x200000 映射到 0x80200000, 2MB大小 */
  pmap_add(0x200000u, 0x80200000u, 2*1024*1024, 0);

  global_test_data_va = (uint32_t *)((uintptr_t)&global_test_data - 0x80000000u);

	write_csr(satp, 8ull << 60 | ((uintptr_t)pte0 >> 12));
	asm volatile ("sfence.vma  x0, x0");

  *global_test_data_va = 200;

#if 1
  /* disable MMU */
  /* 打开MMU之后, 访问串口会失败 */
	write_csr(satp, 0);
	asm volatile ("sfence.vma  x0, x0");
#endif

	printf("global_test_data_va=%p, global_test_data = %d\n", global_test_data_va, global_test_data);

}
