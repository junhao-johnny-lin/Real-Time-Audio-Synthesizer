#include "cyhal.h"
#include "cybsp.h"
#include "cy_pdl.h"
#include "cycfg_capsense.h"
#include "cycfg.h"

#include "GUI.h"
#include "mtb_st7789v.h"
#include "cy8ckit_028_tft_pins.h"

#ifdef USE_AK4954A
#include "mtb_ak4954a.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define SR 16000
#define NS 1024
#define MCLKF 4083000u
#define MCLKD 50.0f
#define SYSCLK 98000000u
#define MCLK P5_0
#define DIV 4u
#define BLUE P11_1
#define CPRI 7U

const mtb_st7789v_pins_t tft_pins={
    .db08=CY8CKIT_028_TFT_PIN_DISPLAY_DB8,
    .db09=CY8CKIT_028_TFT_PIN_DISPLAY_DB9,
    .db10=CY8CKIT_028_TFT_PIN_DISPLAY_DB10,
    .db11=CY8CKIT_028_TFT_PIN_DISPLAY_DB11,
    .db12=CY8CKIT_028_TFT_PIN_DISPLAY_DB12,
    .db13=CY8CKIT_028_TFT_PIN_DISPLAY_DB13,
    .db14=CY8CKIT_028_TFT_PIN_DISPLAY_DB14,
    .db15=CY8CKIT_028_TFT_PIN_DISPLAY_DB15,
    .nrd =CY8CKIT_028_TFT_PIN_DISPLAY_NRD,
    .nwr =CY8CKIT_028_TFT_PIN_DISPLAY_NWR,
    .dc  =CY8CKIT_028_TFT_PIN_DISPLAY_DC,
    .rst =CY8CKIT_028_TFT_PIN_DISPLAY_RST
};

int16_t buf[NS*2];

typedef enum{SINE,SQUARE,SAW}wave_t;
wave_t wave=SINE;

const int16_t sine[64]={
0,1176,2340,3480,4586,5647,6655,7601,
8477,9277,9993,10623,11161,11604,11948,12191,
12331,12367,12299,12127,11852,11476,11002,10433,
9774,9030,8208,7315,6360,5350,4295,3204,
2088,958,-176,-1304,-2415,-3499,-4545,-5543,
-6483,-7356,-8153,-8867,-9490,-10016,-10439,-10755,
-10960,-11052,-11029,-10891,-10640,-10278,-9810,-9240,
-8575,-7821,-6988,-6085,-5122,-4109,-3058,-1980};

const float pitch_table[16]={
110,132,156,185,
220,262,311,370,
440,523,622,740,
880,1047,1245,1480
};

bool scan=0,audio=0,pbtn=0,pcap=0,trem=0,pb1=0;

uint32_t tremc=0;

float phase=0,inc=1,freq=440;

cyhal_pwm_t pwm;
cyhal_i2s_t i2s;
cyhal_clock_t aclk,pll,fll,sys;

#ifdef USE_AK4954A
cyhal_i2c_t i2c;

const cyhal_i2c_cfg_t i2ccfg={
.is_slave=false,
.address=0,
.frequencyhal_hz=400000};
#endif

const cyhal_i2s_pins_t pins={
.sck=P5_1,
.ws=P5_2,
.data=P5_3,
.mclk=NC};

const cyhal_i2s_config_t cfg={
.is_tx_slave=false,
.is_rx_slave=false,
.mclk_hz=0,
.channel_length=32,
.word_length=16,
.sample_rate_hz=SR};

void clk(void),wavegen(void),audio_task(void*arg),cap_task(void*arg);
void draw_ui(void),draw_waveform(void);
static void cap_isr(void);
void endscan(cy_stc_active_scan_sns_t *p);
void i2s_cb(void*a,cyhal_i2s_event_t e);

void draw_waveform(void){

    GUI_SetColor(GUI_DARKGRAY);
    GUI_FillRect(10,200,230,300);

    GUI_SetColor(GUI_CYAN);

    for(int x=0;x<200;x++){

        int idx=(x+tremc)%64;
        int y=250;

        switch(wave){

            case SINE:
                y=250-(sine[idx]/300);
                break;

            case SQUARE:
                y=(idx<32)?220:280;
                break;

            case SAW:
                y=300-((idx*80)/64);
                break;
        }

        GUI_DrawPixel(20+x,y);
    }
}

void draw_ui(void){

    char txt[32];

    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();

    GUI_SetColor(GUI_CYAN);
    GUI_SetFont(GUI_FONT_24_ASCII);

    GUI_GotoXY(20,5);
    GUI_DispString("AUDIO SYNTH");

    GUI_SetColor(GUI_DARKGRAY);
    GUI_DrawHLine(35,0,239);

    GUI_SetFont(GUI_FONT_20_ASCII);

    GUI_SetColor(GUI_YELLOW);

    GUI_GotoXY(10,50);
    GUI_DispString("Wave:");

    GUI_GotoXY(10,90);
    GUI_DispString("Freq:");

    GUI_GotoXY(10,130);
    GUI_DispString("Tremolo:");

    GUI_GotoXY(10,170);
    GUI_DispString("Audio:");

    GUI_GotoXY(130,50);

    switch(wave){

        case SINE:
            GUI_SetColor(GUI_GREEN);
            GUI_DispString("SINE");
            break;

        case SQUARE:
            GUI_SetColor(GUI_BLUE);
            GUI_DispString("SQUARE");
            break;

        case SAW:
            GUI_SetColor(GUI_MAGENTA);
            GUI_DispString("SAW");
            break;
    }

    GUI_SetColor(GUI_WHITE);

	sprintf(txt,"%d Hz",(int)freq);	

    GUI_GotoXY(130,90);
    GUI_DispString(txt);

    GUI_GotoXY(130,130);

    if(trem){
        GUI_SetColor(GUI_GREEN);
        GUI_DispString("ON");
    }else{
        GUI_SetColor(GUI_RED);
        GUI_DispString("OFF");
    }

    GUI_GotoXY(130,170);

    if(audio){
        GUI_SetColor(GUI_GREEN);
        GUI_DispString("PLAY");
    }else{
        GUI_SetColor(GUI_RED);
        GUI_DispString("STOP");
    }

    GUI_SetColor(GUI_DARKGRAY);

    GUI_DrawRect(5,40,235,190);

    GUI_DrawHLine(80,5,235);
    GUI_DrawHLine(120,5,235);
    GUI_DrawHLine(160,5,235);

    GUI_SetColor(GUI_CYAN);
    GUI_DrawRect(10,200,230,300);

    draw_waveform();
}

void wavegen(void){

    for(int i=0;i<NS;i++){

        int idx=((int)phase)%64;
        int16_t s=0;

        switch(wave){

            case SINE:
                s=sine[idx];
                break;

            case SQUARE:
                s=(idx<32)?8000:-8000;
                break;

            case SAW:
                s=-8000+((16000*idx)/63);
                break;
        }

        if(trem){

            int16_t g=sine[((tremc/4)+i)%64]+12000;
            s=(s*g)/24000;
        }

        buf[2*i]=buf[2*i+1]=s;

        phase+=inc;

        if(phase>=64)
            phase-=64;
    }

    tremc++;
}

void audio_task(void*arg){

    (void)arg;

    for(;;){

        if(audio){

            if(!cyhal_i2s_is_write_pending(&i2s)){

                cyhal_i2s_start_tx(&i2s);

                wavegen();

                cyhal_i2s_write_async(&i2s,buf,NS*2);

                cyhal_gpio_write(BLUE,CYBSP_LED_STATE_ON);
            }

        }else{

            cyhal_gpio_write(BLUE,CYBSP_LED_STATE_OFF);
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void cap_task(void*arg){

    (void)arg;

    for(;;){

        bool btn=(cyhal_gpio_read(CYBSP_USER_BTN)==CYBSP_BTN_PRESSED);

        if(btn&&!pbtn){

            audio=!audio;
            draw_ui();
        }

        pbtn=btn;

        if(scan){

            scan=0;

            Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);

            uint32_t b0=Cy_CapSense_IsSensorActive(
                CY_CAPSENSE_BUTTON0_WDGT_ID,
                CY_CAPSENSE_BUTTON0_SNS0_ID,
                &cy_capsense_context);

            uint32_t b1=Cy_CapSense_IsSensorActive(
                CY_CAPSENSE_BUTTON1_WDGT_ID,
                CY_CAPSENSE_BUTTON1_SNS0_ID,
                &cy_capsense_context);

            cy_stc_capsense_touch_t *sl=
                Cy_CapSense_GetTouchInfo(
                CY_CAPSENSE_LINEARSLIDER0_WDGT_ID,
                &cy_capsense_context);

            if(b0&&!pcap){

                wave++;

                if(wave>SAW)
                    wave=SINE;

                draw_ui();
            }

            pcap=b0;

            if(b1&&!pb1){

                trem=!trem;
                draw_ui();
            }

            pb1=b1;

            if(sl->numPosition){

                uint16_t pos=sl->ptrPosition->x;

                uint16_t res=
                cy_capsense_context
                .ptrWdConfig[
                CY_CAPSENSE_LINEARSLIDER0_WDGT_ID]
                .xResolution;

                uint8_t idx=(pos*15)/res;

                freq=pitch_table[idx];

                inc=(freq*64.0f)/SR;

                draw_ui();
            }

            Cy_CapSense_ScanAllWidgets(&cy_capsense_context);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

cy_rslt_t initCapSense(void){

    cy_status s=Cy_CapSense_Init(&cy_capsense_context);

    if(s!=CYRET_SUCCESS)
        return s;

    cyhal_system_set_isr(
        csd_interrupt_IRQn,
        csd_interrupt_IRQn,
        CPRI,
        &cap_isr);

    NVIC_ClearPendingIRQ(csd_interrupt_IRQn);

    NVIC_EnableIRQ(csd_interrupt_IRQn);

    s=Cy_CapSense_Enable(&cy_capsense_context);

    if(s!=CYRET_SUCCESS)
        return s;

    Cy_CapSense_RegisterCallback(
        CY_CAPSENSE_END_OF_SCAN_E,
        endscan,
        &cy_capsense_context);

    return CY_RSLT_SUCCESS;
}

static void cap_isr(void){

    Cy_CapSense_InterruptHandler(
        CYBSP_CSD_HW,
        &cy_capsense_context);
}

void endscan(cy_stc_active_scan_sns_t *p){

    (void)p;
    scan=1;
}

void i2s_cb(void*a,cyhal_i2s_event_t e){

    (void)a;
    (void)e;
}

void clk(void){

    cyhal_clock_reserve(&pll,&CYHAL_CLOCK_PLL[0]);

    cyhal_clock_set_frequency(&pll,SYSCLK,NULL);

    cyhal_clock_set_enabled(&pll,true,true);

    cyhal_clock_reserve(&aclk,&CYHAL_CLOCK_HF[1]);

    cyhal_clock_set_source(&aclk,&pll);

    cyhal_clock_set_divider(&aclk,DIV);

    cyhal_clock_set_enabled(&aclk,true,true);

    cyhal_clock_reserve(&sys,&CYHAL_CLOCK_HF[0]);

    cyhal_clock_set_source(&sys,&pll);

    cyhal_clock_reserve(&fll,&CYHAL_CLOCK_FLL);

    cyhal_clock_set_enabled(&fll,false,true);
}

int main(void){

    cy_rslt_t r=cybsp_init();

    if(r!=CY_RSLT_SUCCESS)
        CY_ASSERT(0);

    __enable_irq();

    r=mtb_st7789v_init8(&tft_pins);

    CY_ASSERT(r==CY_RSLT_SUCCESS);

    GUI_Init();

    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();

    GUI_SetColor(GUI_CYAN);
    GUI_SetFont(GUI_FONT_32_ASCII);

    GUI_GotoXY(25,100);
    GUI_DispString("SYNTH");

    GUI_GotoXY(10,150);
    GUI_DispString("INITIALIZING...");

    cyhal_system_delay_ms(1500);

    clk();

    draw_ui();

    cyhal_gpio_init(
        BLUE,
        CYHAL_GPIO_DIR_OUTPUT,
        CYHAL_GPIO_DRIVE_STRONG,
        CYBSP_LED_STATE_OFF);

    cyhal_gpio_init(
        CYBSP_USER_BTN,
        CYHAL_GPIO_DIR_INPUT,
        CYHAL_GPIO_DRIVE_PULLUP,
        CYBSP_BTN_OFF);

    cyhal_pwm_init(&pwm,MCLK,NULL);

    cyhal_pwm_set_duty_cycle(&pwm,MCLKD,MCLKF);

    cyhal_pwm_start(&pwm);

    cyhal_system_delay_ms(1);

    cyhal_i2s_init(&i2s,&pins,NULL,&cfg,&aclk);

    cyhal_i2s_register_callback(&i2s,i2s_cb,NULL);

    cyhal_i2s_enable_event(
        &i2s,
        CYHAL_I2S_ASYNC_TX_COMPLETE,
        CYHAL_ISR_PRIORITY_DEFAULT,
        true);

#ifdef USE_AK4954A

    cyhal_i2c_init(
        &i2c,
        CYBSP_I2C_SDA,
        CYBSP_I2C_SCL,
        NULL);

    cyhal_i2c_configure(&i2c,&i2ccfg);

    r=mtb_ak4954a_init(&i2c);

    if(r!=CY_RSLT_SUCCESS)
        NVIC_SystemReset();

    mtb_ak4954a_activate();

    mtb_ak4954a_adjust_volume(
        AK4954A_HP_VOLUME_DEFAULT);

#endif

    initCapSense();

    Cy_CapSense_ScanAllWidgets(&cy_capsense_context);

    xTaskCreate(cap_task,"Cap",2048,NULL,1,NULL);

    xTaskCreate(audio_task,"Aud",2048,NULL,1,NULL);

    vTaskStartScheduler();

    for(;;);
}