#include "display.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *TAG = "DISPLAY";
static i2c_master_bus_handle_t i2c_bus = NULL;
static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;
static SemaphoreHandle_t oled_mutex = NULL;
static bool oled_ready = false;
static face_state_t current_face_state = FACE_IDLE;
static uint8_t face_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static void draw_buffer_locked(const uint8_t *buffer)
{
    if (!panel || !buffer) return;
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, OLED_WIDTH, OLED_HEIGHT, buffer);
    if (err != ESP_OK) ESP_LOGW(TAG, "OLED draw gagal: %s", esp_err_to_name(err));
}

void oled_init(void)
{
    if (oled_ready) return;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = OLED_SDA_PIN, .scl_io_num = OLED_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .intr_priority = 0,
        .trans_queue_depth = 0, .flags = {.enable_internal_pullup = 1, .allow_pd = 0},
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Gagal membuat I2C OLED: %s", esp_err_to_name(err)); return; }

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDR, .scl_speed_hz = 400 * 1000,
        .control_phase_bytes = 1, .dc_bit_offset = 6, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .on_color_trans_done = NULL, .user_ctx = NULL,
        .flags = {.dc_low_on_data = 0, .disable_control_phase = 0},
    };
    err = esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &panel_io);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Gagal membuat panel I2C OLED: %s", esp_err_to_name(err)); return; }

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.bits_per_pixel = 1;
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {.height = OLED_HEIGHT};
    panel_config.vendor_config = &ssd1306_config;

    ESP_LOGI(TAG, "Install SSD1306 driver");
    err = esp_lcd_new_panel_ssd1306(panel_io, &panel_config, &panel);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Gagal install SSD1306: %s", esp_err_to_name(err)); return; }
    if ((err = esp_lcd_panel_reset(panel)) != ESP_OK) { ESP_LOGE(TAG, "OLED reset gagal: %s", esp_err_to_name(err)); return; }
    if ((err = esp_lcd_panel_init(panel)) != ESP_OK) { ESP_LOGE(TAG, "OLED init gagal: %s", esp_err_to_name(err)); return; }
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    oled_mutex = xSemaphoreCreateMutex();
    if (!oled_mutex) { ESP_LOGE(TAG, "Gagal membuat mutex OLED"); return; }
    oled_ready = true;
    memset(face_buffer, 0, sizeof(face_buffer));
    display_render_buffer(face_buffer);
    ESP_LOGI(TAG, "XiaoZhi SSD1306 128x64 siap: SDA=%d SCL=%d ADDR=0x%02X SPEED=400kHz MIRROR=XY",
             OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);
}

void display_render_buffer(const uint8_t *buffer)
{
    if (!buffer) return;
    if (!oled_ready) oled_init();
    if (!oled_ready || !oled_mutex) return;
    if (xSemaphoreTake(oled_mutex, pdMS_TO_TICKS(250)) != pdTRUE) { ESP_LOGW(TAG, "OLED mutex timeout"); return; }
    draw_buffer_locked(buffer);
    xSemaphoreGive(oled_mutex);
}

void display_status(const char *text)
{
    if (!oled_ready) oled_init();
    ESP_LOGI(TAG, "[OLED STATUS]: %s", text ? text : "(null)");
}

void face_set_state(face_state_t state) { current_face_state = state; }
face_state_t face_get_state(void) { return current_face_state; }

static void pixel(int x, int y, bool on = true)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    uint8_t &b = face_buffer[x + (y >> 3) * OLED_WIDTH];
    uint8_t m = (uint8_t)(1U << (y & 7));
    if (on) b |= m; else b &= (uint8_t)~m;
}
static void fill_rect(int x,int y,int w,int h,bool on)
{ for(int yy=y;yy<y+h;++yy) for(int xx=x;xx<x+w;++xx) pixel(xx,yy,on); }
static void fill_circle(int cx,int cy,int r,bool on=true)
{
    for(int y=-r;y<=r;++y){ int dx=(int)sqrtf((float)(r*r-y*y)); for(int x=-dx;x<=dx;++x) pixel(cx+x,cy+y,on); }
}
static void line(int x0,int y0,int x1,int y1)
{
    int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,err=dx+dy;
    for(;;){pixel(x0,y0);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
static void draw_mochi_eye(int cx,int cy,int pupil_x,bool closed,bool sleepy)
{
    const int r=14;
    if(closed){
        fill_circle(cx,cy-6,r,true); fill_circle(cx,cy-11,r,false);
        fill_rect(cx-r-1,cy-r-12,r*2+2,r+6,false); return;
    }
    fill_circle(cx,cy,r,true);
    if(sleepy) fill_rect(cx-r,cy-r,r*2,10,false);
    fill_circle(pupil_x,cy,7,false);
    fill_circle(pupil_x-2,cy-3,3,true);
}

void display_render_mochi(int expr,int step,int sx,int sy,int arahLirik)
{
    if(!oled_ready) oled_init();
    if(!oled_ready) return;
    memset(face_buffer,0,sizeof(face_buffer));
    const int L=34+sx, R=94+sx, Y=28+sy, r=14;
    int lp=L, rp=R;
    if(arahLirik==1){lp-=3;rp-=3;} else if(arahLirik==2){lp+=3;rp+=3;}
    if(expr==2){
        if(step==1){
            fill_circle(L,Y+6,r,true); fill_circle(L,Y+11,r,false); fill_rect(L-r-1,Y+6,r*2+2,r+6,false);
            fill_circle(R,Y+6,r,true); fill_circle(R,Y+11,r,false); fill_rect(R-r-1,Y+6,r*2+2,r+6,false);
        } else { draw_mochi_eye(L,Y,lp,false,false); draw_mochi_eye(R,Y,rp,false,false); }
    } else if(expr==6){
        if(step==1){
            fill_circle(L,Y,r,true); fill_rect(L-r,Y-r,r*2,r+4,false);
            fill_circle(R,Y,r,true); fill_rect(R-r,Y-r,r*2,r+4,false);
        } else {
            draw_mochi_eye(L,Y,lp,false,false); fill_rect(L-r,Y-r,r*2,10,false);
            draw_mochi_eye(R,Y,rp,false,false); fill_rect(R-r,Y-r,r*2,10,false);
        }
    } else if(expr==99){
        fill_circle(L,Y,r,true); fill_circle(lp,Y,9,false);
        fill_circle(R,Y,r,true); fill_circle(rp,Y,9,false);
    } else if(step==1){
        fill_circle(L,Y-6,r,true); fill_circle(L,Y-11,r,false); fill_rect(L-r-1,Y-r-12,r*2+2,r+6,false);
        fill_circle(R,Y-6,r,true); fill_circle(R,Y-11,r,false); fill_rect(R-r-1,Y-r-12,r*2+2,r+6,false);
    } else if(expr==1){
        draw_mochi_eye(L,Y,lp,false,false); fill_circle(L,Y+24,r+2,false);
        draw_mochi_eye(R,Y,rp,false,false); fill_circle(R,Y+24,r+2,false);
    } else if(expr==3){
        draw_mochi_eye(L,Y,lp,false,false);
        fill_circle(R,Y-6,r,true); fill_circle(R,Y-11,r,false); fill_rect(R-r-1,Y-r-12,r*2+2,r+6,false);
    } else {
        draw_mochi_eye(L,Y,lp,false,false); draw_mochi_eye(R,Y,rp,false,false);
    }
    display_render_buffer(face_buffer);
}

void face_render(void)
{
    int expr=0;
    switch(current_face_state){
        case FACE_LISTENING: expr=1; break;
        case FACE_THINKING: expr=0; break;
        case FACE_SPEAKING: expr=2; break;
        case FACE_HAPPY: expr=2; break;
        case FACE_SAD: expr=6; break;
        case FACE_ERROR: expr=99; break;
        case FACE_SLEEP: expr=0; break;
        case FACE_IDLE: default: expr=0; break;
    }
    display_render_mochi(expr, current_face_state==FACE_SLEEP ? 1 : 0, 0, 0, 0);
}
