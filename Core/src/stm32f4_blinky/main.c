#include "stm32f4xx.h"
#include <stdint.h>

static void delay(volatile uint32_t count) {
    while(count--) __asm__("nop");
}

int main(void) {
    // 啟動 GPIOC 時鐘
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    // 設定 PC13 為輸出
    GPIOC->MODER &= ~(3 << (13 * 2));
    GPIOC->MODER |=  (1 << (13 * 2));

    while (1) {
        GPIOC->ODR ^= (1 << 13);  // toggle PC13
        delay(500000);
    }
}
