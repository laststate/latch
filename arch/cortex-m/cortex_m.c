#include "cortex_m.h"
#include "laststate/latch.h"
#include "laststate/noinit.h"
#define REG32(address) (*(volatile const uint32_t *)(uintptr_t)(address))
#define REG32W(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define LS_STACK_CANARY 0x51ac7e5au
#if defined(__GNUC__)
#define LS_ALIGN32 __attribute__((aligned(32)))
#elif defined(_MSC_VER)
#define LS_ALIGN32 __declspec(align(32))
#else
#define LS_ALIGN32
#endif
typedef struct {uint8_t guard[32];uint8_t stack[LS_EMERGENCY_STACK_SIZE];} ls_emergency_memory_t;
LS_NOINIT LS_ALIGN32 static ls_emergency_memory_t emergency_memory;
uintptr_t ls_cortex_m_emergency_stack_top=(uintptr_t)(emergency_memory.stack+sizeof emergency_memory.stack);
ls_cortex_m_saved_t ls_cortex_m_saved_context;

void ls_cortex_m_init(void){for(size_t i=0;i<sizeof emergency_memory.stack;i++)emergency_memory.stack[i]=0xa5;*(uint32_t *)(void *)emergency_memory.stack=LS_STACK_CANARY;}
bool ls_cortex_m_emergency_stack_ok(void){return *(const uint32_t *)(const void *)emergency_memory.stack==LS_STACK_CANARY;}
ls_result_t ls_cortex_m_configure_emergency_stack_mpu(uint8_t region_number){
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    if(region_number>=8u)return LS_EINVAL;REG32W(0xe000ed98u)=region_number;REG32W(0xe000ed9cu)=(uint32_t)(uintptr_t)emergency_memory.guard;REG32W(0xe000eda0u)=(1u<<28)|(4u<<1)|1u;REG32W(0xe000ed94u)|=5u;__asm volatile("dsb 0xf\n isb 0xf" ::: "memory");return LS_OK;
#elif defined(__ARM_ARCH_8M_MAIN__)
    if(region_number>=16u)return LS_EINVAL;REG32W(0xe000ed98u)=region_number;REG32W(0xe000edc0u)=0x44u;REG32W(0xe000ed9cu)=((uint32_t)(uintptr_t)emergency_memory.guard&~31u)|(2u<<1)|1u;REG32W(0xe000eda0u)=(((uint32_t)(uintptr_t)emergency_memory.guard+31u)&~31u)|1u;REG32W(0xe000ed94u)|=5u;__asm volatile("dsb 0xf\n isb 0xf" ::: "memory");return LS_OK;
#else
    (void)region_number;return LS_ENOTSUP;
#endif
}
void ls_cortex_m_enable_configurable_faults(bool secure_fault){uint32_t mask=(1u<<16)|(1u<<17)|(1u<<18);if(secure_fault)mask|=1u<<19;REG32W(0xe000ed24u)|=mask;}
void ls_cortex_m_configure_fpu_lazy_stacking(bool enabled){
#if defined(__ARM_FP) && (__ARM_FP != 0)
    if(enabled)REG32W(0xe000ef34u)|=(1u<<31)|(1u<<30);else REG32W(0xe000ef34u)&=~(1u<<30);
#else
    (void)enabled;
#endif
}
void ls_cortex_m_fault_from_saved(const uint32_t *raw_frame,const ls_cortex_m_saved_t *saved){
    ls_arch_context_t context={0};context.architecture=LS_ARCH_CORTEX_M;context.fault=(ls_fault_kind_t)saved->fault_kind;context.msp=saved->msp;context.psp=saved->psp;context.control=saved->control;context.primask=saved->primask;context.basepri=saved->basepri;context.faultmask=saved->faultmask;context.exc_return=saved->exc_return;
    const bool extended=(saved->exc_return&(1u<<4))==0;const uint32_t *core_frame=extended?raw_frame+18:raw_frame;
    if(core_frame){context.r[0]=core_frame[0];context.r[1]=core_frame[1];context.r[2]=core_frame[2];context.r[3]=core_frame[3];for(unsigned i=4;i<=11;i++)context.r[i]=saved->r4_r11[i-4];context.r[12]=core_frame[4];context.lr=core_frame[5];context.pc=core_frame[6];context.xpsr=core_frame[7];for(unsigned i=0;i<13;i++)context.registers[i]=context.r[i];context.registers[14]=context.lr;context.registers[15]=context.pc;}
    context.cfsr=REG32(0xe000ed28u);context.hfsr=REG32(0xe000ed2cu);context.dfsr=REG32(0xe000ed30u);context.afsr=REG32(0xe000ed3cu);context.mmfar=REG32(0xe000ed34u);context.bfar=REG32(0xe000ed38u);context.shcsr=REG32(0xe000ed24u);context.icsr=REG32(0xe000ed04u);context.vtor=REG32(0xe000ed08u);
    if(context.fault==LS_FAULT_SECURE){context.sfsr=REG32(0xe000ede4u);context.sfar=REG32(0xe000ede8u);}
#if defined(__ARM_FP) && (__ARM_FP != 0)
    context.fpu_lazy=(REG32(0xe000ef34u)&1u)!=0;if(extended&&raw_frame){for(unsigned i=0;i<16;i++)context.s[i]=raw_frame[i];context.fpscr=raw_frame[16];context.has_fpu=true;}
#endif
    if(!ls_cortex_m_emergency_stack_ok())(void)ls_capture_minimal(&context);else (void)ls_capture_cpu_context(&context);for(;;){}
}
