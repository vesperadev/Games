#include "OLED.h"
#include "font.h"
#include "cmsis_os.h"
#include "main.h"
#include "i2c.h"

void StartOLEDTask(void *argument){
    OLED_Init();
    for(;;)
    {
        OLED_NewFrame();
        OLED_PrintString(0, 0, "Hello World!", &font16x16, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        osDelay(1000);
        
        OLED_NewFrame();
        OLED_ShowFrame();
        osDelay(1000);
    }
}
