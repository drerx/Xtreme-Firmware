#include "api-hal-interrupt.h"
#include "api-hal-irda.h"

#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_gpio.h>

#include <stdio.h>
#include <furi.h>
#include "main.h"
#include "api-hal-pwm.h"

static struct{
    TimerISRCallback callback;
    void *ctx;
} timer_irda;

typedef enum{
    TimerIRQSourceCCI1,
    TimerIRQSourceCCI2,
} TimerIRQSource;

static void api_hal_irda_handle_capture(TimerIRQSource source)
{
    uint32_t duration = 0;
    bool level = 0;

    switch (source) {
    case TimerIRQSourceCCI1:
        duration = LL_TIM_OC_GetCompareCH1(TIM2);
        LL_TIM_SetCounter(TIM2, 0);
        level = 1;
        break;
    case TimerIRQSourceCCI2:
        duration = LL_TIM_OC_GetCompareCH2(TIM2);
        LL_TIM_SetCounter(TIM2, 0);
        level = 0;
        break;
    default:
        furi_check(0);
    }

    if (timer_irda.callback)
        timer_irda.callback(timer_irda.ctx, level, duration);
}

static void api_hal_irda_isr() {
    if(LL_TIM_IsActiveFlag_CC1(TIM2) == 1) {
        LL_TIM_ClearFlag_CC1(TIM2);

        if(READ_BIT(TIM2->CCMR1, TIM_CCMR1_CC1S)) {
            // input capture
            api_hal_irda_handle_capture(TimerIRQSourceCCI1);
        } else {
            // output compare
            //  HAL_TIM_OC_DelayElapsedCallback(htim);
            //  HAL_TIM_PWM_PulseFinishedCallback(htim);
        }
    }
    if(LL_TIM_IsActiveFlag_CC2(TIM2) == 1) {
        LL_TIM_ClearFlag_CC2(TIM2);

        if(READ_BIT(TIM2->CCMR1, TIM_CCMR1_CC2S)) {
            // input capture
            api_hal_irda_handle_capture(TimerIRQSourceCCI2);
        } else {
            // output compare
            //  HAL_TIM_OC_DelayElapsedCallback(htim);
            //  HAL_TIM_PWM_PulseFinishedCallback(htim);
        }
    }
}

void api_hal_irda_rx_irq_init(void)
{
    LL_TIM_InitTypeDef TIM_InitStruct = {0};

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    /**TIM2 GPIO Configuration
    PA0   ------> TIM2_CH1
    */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    TIM_InitStruct.Prescaler = 64 - 1;
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload = 0xFFFFFFFF;
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    LL_TIM_Init(TIM2, &TIM_InitStruct);
    LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_EnableARRPreload(TIM2);
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
    LL_TIM_DisableMasterSlaveMode(TIM2);
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_FALLING);
    LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_INDIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);

    LL_TIM_EnableIT_CC1(TIM2);
    LL_TIM_EnableIT_CC2(TIM2);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

    LL_TIM_SetCounter(TIM2, 0);
    LL_TIM_EnableCounter(TIM2);

    api_hal_interrupt_set_timer_isr(TIM2, api_hal_irda_isr);
    NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
    NVIC_EnableIRQ(TIM2_IRQn);
}

void api_hal_irda_rx_irq_deinit(void) {
    LL_TIM_DeInit(TIM2);
    api_hal_interrupt_set_timer_isr(TIM2, NULL);
}

bool api_hal_irda_rx_irq_is_busy(void) {
    return (LL_TIM_IsEnabledIT_CC1(TIM2) || LL_TIM_IsEnabledIT_CC2(TIM2));
}

void api_hal_irda_rx_irq_set_callback(TimerISRCallback callback, void *ctx) {
    furi_check(callback);

    timer_irda.callback = callback;
    timer_irda.ctx = ctx;
}

void api_hal_irda_pwm_set(float value, float freq) {
    hal_pwmn_set(value, freq, &IRDA_TX_TIM, IRDA_TX_CH);
}

void api_hal_irda_pwm_stop() {
    hal_pwmn_stop(&IRDA_TX_TIM, IRDA_TX_CH);
}
