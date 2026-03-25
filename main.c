#include "main.h"
#include "Thread.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "Font/font_data.h"
#include "display/render/context.h"
#include "BackBuffer.h"

int main() {
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;
    uint16_t summcolor=0;
    uint32_t data;
    // Точки для кривой Безье
    int points_x[60];
    int points_y[60];
    size_t num_points = 1600;
    uint16_t color = reverse(0b0000011111100000);
    stdio_init_all();
    
    // Настройка GPIO25 как выход
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    // Включение светодиода
    gpio_put(25, 1);

    multicore_launch_core1(coreEntry); //Запускаю в ядре 1 процесс вывода на экран
    float x=0.0;
    float freq=1.0;
    bool minmax=false;
    render_ctx_t rc;
    wchar_t buffer[100];
    while (x<500)
    {
        data = multicore_fifo_pop_blocking();
        
        fillBufer(frame_buffer,reverse(0x4A69));//
        grid(20,20,40,0x634d);
        generate_sine_wave_points(num_points, 50, freq, 0, HEIGHT / 2,x);
        render_begin(&rc, frame_buffer, WIDTH, HEIGHT);
        swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"Value of pi: %.2f", temperature);
        draw_string(&rc, 20,110,buffer,0b1111111111111111); //
        multicore_fifo_push_blocking(0); //Экран 0 нарисован       
        x+=0.03;
        if (minmax)
        {
            if (freq<=0.0){
            minmax=false;}
            else freq-=0.05;
        }
        else
        {
            if (freq>=10.0){
            minmax=true;}
            else freq+=0.05;
        }

        

        //sleep_ms(20);   
    }
    
    while (1) {
        tight_loop_contents();
    }
}

void fillBufer (uint16_t* buffer,uint16_t color){
    for (uint16_t  y = 0; y < HEIGHT; y++)
    {
        for (uint16_t x = 0; x < WIDTH; x++)
        {
            buffer[y*WIDTH+x]=color;
        }        
    }
    
}
