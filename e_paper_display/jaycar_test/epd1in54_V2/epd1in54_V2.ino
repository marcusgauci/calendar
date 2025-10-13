#include <SPI.h>
#include "epd1in54_V2.h"
#include "epdpaint.h"
#include <stdio.h>
#include "icons.h"

Epd epd;
unsigned char image[1024];
Paint paint(image, 0, 0);

unsigned long time_start_ms;
unsigned long time_now_s;
#define COLORED     0
#define UNCOLORED   1

const unsigned char* faces[5] = {happy_1, happy_2, neutral_3, sad_4, sad_5};

void set_background();
void set_time(int hh, int mm);
void set_date(int dd, int mm, int yy);
void set_message(char* buf, int num_chars);
void set_moisture(int moisture);
void set_light(int light);
void set_temp(int temp);
void draw_bitmap(Paint* paint, const unsigned char* bitmap,
                int x0, int y0, int width, int height, int colored);

void setup()
{
  int state = 1;

  epd.HDirInit();
  epd.Clear();

  paint.SetRotate(ROTATE_270);

  set_background();
  set_time(11,11);
  set_date(12,13,13);
  set_message("Get good grade on ESI project", 29);
  set_moisture(23);
  set_light(11);
  set_temp(78);  

  draw_bitmap(&paint, faces[state], 100, 190, 40, 40, COLORED);

  //draw face
  // paint.SetWidth(52);
  // paint.SetHeight(52);
  // paint.Clear(UNCOLORED);
  // paint.DrawCircle(26, 26, 24, COLORED);
  // epd.SetFrameMemory(paint.GetImage(), 80, 200, paint.GetWidth(), paint.GetHeight());

  epd.DisplayFrame();

}

void loop()
{

}

void set_time(int hh, int mm){
  char buffer[5];
  if(hh < 10){
    sprintf(buffer, " %d:%02d", hh, mm);
  } else {
    sprintf(buffer, "%02d:%02d", hh, mm);
  }

  paint.SetWidth(16);
  paint.SetHeight(72);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, buffer, &Font20, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 0, 72, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
}

void set_date(int dd, int mm, int yy){
  char buffer[8];
  if(dd < 10){
    if(mm < 10){
      sprintf(buffer, "0%d.0%d.%02d", dd, mm, yy);
    } else {
      sprintf(buffer, "0%d.%02d.%02d", dd, mm, yy);
    }
  } else {
    if(mm < 10){
      sprintf(buffer, "%02d.0%d.%02d", dd, mm, yy);
    } else {
      sprintf(buffer, "%02d.%02d.%02d", dd, mm, yy);
    }
  }

  // draw date
  paint.SetWidth(16);
  paint.SetHeight(112);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, buffer, &Font20, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 0, 200, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
}

void set_message(char* buf, int num_chars){
  paint.SetWidth(32); 
  paint.SetHeight(180);
  paint.Clear(UNCOLORED);
  
  if(num_chars < 16){
    paint.DrawStringAt(0, 0, buf, &Font16, COLORED);
  } else {
    char line_1[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
    char line_2[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'}; 
    for(int i = 0; i < 15; i++){
      line_1[i] = buf[i];
    }
    
    for(int i = 15; i < num_chars; i++){
      line_2[i - 15] = buf[i];
    }

    paint.DrawStringAt(0, 0, line_1, &Font16, COLORED);
    paint.DrawStringAt(0, 14, line_2, &Font16, COLORED);
  }

  epd.SetFrameMemory(paint.GetImage(), 46, 200, paint.GetWidth(), paint.GetHeight());
}

void set_moisture(int moisture){
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", moisture);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 32, paint.GetWidth(), paint.GetHeight());
}

void set_light(int light){
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", light);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 82, paint.GetWidth(), paint.GetHeight());
}

void set_temp(int temp){
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", temp);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 132, paint.GetWidth(), paint.GetHeight());
}

void set_background(){
  paint.SetWidth(64);
  paint.SetHeight(64);

  paint.Clear(UNCOLORED);
  paint.DrawLine(39, 0, 40, 32, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 0, 102, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(200);
  paint.Clear(UNCOLORED);
  paint.DrawHorizontalLine(0, 5, 200, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 16, 200, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(200);
  paint.Clear(UNCOLORED);
  paint.DrawHorizontalLine(0, 5, 200, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 72, 200, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(150);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "To Do Today:", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 30, 150, paint.GetWidth(), paint.GetHeight());

  // plant health
  paint.SetWidth(16);
  paint.SetHeight(150);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "Plant Health:", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 86, 150, paint.GetWidth(), paint.GetHeight());

    //draw icons
  paint.SetWidth(40);
  paint.SetHeight(40);
  paint.Clear(UNCOLORED);
  paint.DrawRectangle(0, 0, 25, 35, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 155, 200, paint.GetWidth(), paint.GetHeight());

  draw_bitmap(&paint, thermometer_img, 114, 143, 40, 40, COLORED);
  draw_bitmap(&paint, sun_img, 114, 93, 40, 40, COLORED);
  draw_bitmap(&paint, humidity_img, 114, 43, 40, 40, COLORED);
  draw_bitmap(&paint, plant_img, 155, 190, 40, 40, COLORED);

  //plant units
  paint.SetWidth(20);
  paint.SetHeight(35);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "%", &Font20, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 52, paint.GetWidth(), paint.GetHeight());

  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "lux", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 89, paint.GetWidth(), paint.GetHeight());

  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "C", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 152, paint.GetWidth(), paint.GetHeight());

  // bullet point
  paint.SetWidth(12);
  paint.SetHeight(12);
  paint.Clear(UNCOLORED);
  paint.DrawFilledCircle(4, 6, 2, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 46, 20, paint.GetWidth(), paint.GetHeight());
}

void draw_bitmap(Paint* paint, const unsigned char* bitmap,
                int x0, int y0, int width, int height, int colored)
{
    paint->SetWidth(width);
    paint->SetHeight(height);
    paint->Clear(UNCOLORED);

    int byteWidth = (width + 7) / 8;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int byteIndex = y * byteWidth + (x / 8);
            uint8_t byte = pgm_read_byte(&bitmap[byteIndex]); // Read from PROGMEM safely

            // image2cpp exports MSB first → test (0x80 >> (x % 8))
            if (!(byte & (0x80 >> (x % 8)))) {
                paint->DrawPixel(x, y, colored);
            }
        }
    }

    epd.SetFrameMemory(paint->GetImage(), x0, y0, paint->GetWidth(), paint->GetHeight());
}
