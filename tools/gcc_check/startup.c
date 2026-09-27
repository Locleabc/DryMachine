#include <stdint.h>
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void); void SystemInit(void);
void Default_Handler(void){ for(;;); }
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void Reset_Handler(void){ uint32_t *s=&_sidata,*d=&_sdata; while(d<&_edata)*d++=*s++; for(d=&_sbss;d<&_ebss;)*d++=0; SystemInit(); main(); for(;;); }
__attribute__((section(".isr_vector"))) void (*const vectors[])(void) = { (void(*)(void))&_estack, Reset_Handler, NMI_Handler, HardFault_Handler,
 0,0,0,0,0,0,0,0,0,0,0, SysTick_Handler };
