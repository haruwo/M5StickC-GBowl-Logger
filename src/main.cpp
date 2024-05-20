#include <Arduino.h>
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LovyanGFX.h>
#include <LGFX_AUTODETECT.hpp>

static LGFX lcd;
static LGFX_Sprite sprite(&lcd);
static auto &SerialUSB = Serial;

#define debug_begin SerialUSB.begin
#define debug_println SerialUSB.println
#define debug_print SerialUSB.print
#define debug_printf SerialUSB.printf

void drawCircles()
{
  sprite.clear();

  // グリッド幅
  const int grid_length = min(sprite.width(), sprite.height()) / (4 * 2);
  const auto grid_color = LIGHTGREY;
  const int centor_x = sprite.width() / 2;
  const int centor_y = sprite.height() / 2;

  for (int i = grid_length; i < sprite.width(); i += grid_length)
  {
    sprite.drawCircle(centor_x, centor_y, i, grid_color);
  }

  // draw cross
  sprite.drawLine(0, centor_y, sprite.width(), centor_y, grid_color);
  sprite.drawLine(centor_x, 0, centor_x, sprite.height(), grid_color);

  sprite.pushSprite(&lcd, 0, 0);
}

struct Point3D
{
  float x;
  float y;
  float z;
};

Point3D accZero;

void drawInformation()
{
  sprite.clear();
  sprite.setTextColor(WHITE, BLACK);
  sprite.setTextSize(1);
  sprite.setFont(&AsciiFont8x16);
  sprite.setCursor(0, 0);
  sprite.println("Information");
  sprite.println("----------------");
  sprite.println("Acceleration");
  sprite.println("----------------");

  Point3D acc;
  M5.Imu.getAccelData(&acc.x, &acc.y, &acc.z);
  sprite.print("X: ");
  sprite.println(acc.x - accZero.x);
  sprite.print("Y: ");
  sprite.println(acc.y - accZero.y);
  sprite.print("Z: ");
  sprite.println(acc.z - accZero.z);
  sprite.pushSprite(&lcd, 0, 0);
}

void resetAcceleration()
{
  M5.Imu.getAccelData(&accZero.x, &accZero.y, &accZero.z);
}

void setupSprite()
{
  sprite.createSprite(lcd.width(), lcd.height()); // Create a sprite with width 80 and height 160
  sprite.setTextColor(WHITE, BLACK);
  sprite.setTextSize(2);
  sprite.setFont(&AsciiFont8x16);
  sprite.fillSprite(BLACK);
  sprite.setCursor(16, 16);
  sprite.print("Sprite Ready");
  debug_println("Sprite Ready");
  sprite.pushSprite(0, 0); // Push the sprite to the display at position (0, 0)
}

void setup()
{
  M5.begin();

  debug_begin(115200);

  M5.IMU.Init();
  resetAcceleration();

  lcd.init();
  lcd.setRotation(3);
  lcd.fillScreen(BLACK);
  lcd.setTextColor(WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(0, 0);
  debug_println("IMU & Display Ready");

  setupSprite(); // Initialize and display the sprite
}

void loop()
{
  M5.update();

  if (M5.BtnA.pressedFor(1500))
  {
    resetAcceleration();
  }
  drawInformation();
  // drawCircles();
  delay(250);
}
