#include <Arduino.h>
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LovyanGFX.h>
#include <LGFX_AUTODETECT.hpp>
#include <deque>
#include <functional>

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

  Point3D operator-=(const Point3D &rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  Point3D operator-(const Point3D &rhs) const
  {
    return Point3D{x - rhs.x, y - rhs.y, z - rhs.z};
  }
};

template <typename T>
struct LogEntry
{
  T data;
  time_t timestamp;
};

class AccelerationLog
{
public:
  AccelerationLog(std::function<void(float *, float *, float *)> getAccelDataFunc, int max_log_size = 16)
      : getAccelData(getAccelDataFunc), accZero{0, 0, 0}, max_log_size(max_log_size)
  {
  }

  void init()
  {
    reset();
  }

  void reset()
  {
    getAccelData(&accZero.x, &accZero.y, &accZero.z);
    accLog.clear();
  }

  void update(time_t timestamp = time(NULL))
  {
    Point3D acc;
    getAccelData(&acc.x, &acc.y, &acc.z);
    accLog.push_back({acc - accZero, timestamp});
    if (accLog.size() > max_log_size)
    {
      accLog.pop_front();
    }
  }

  std::deque<LogEntry<Point3D>>::const_reverse_iterator begin() const
  {
    return accLog.rbegin();
  }

  std::deque<LogEntry<Point3D>>::const_reverse_iterator end() const
  {
    return accLog.rend();
  }

private:
  const int max_log_size;
  std::function<void(float *, float *, float *)> getAccelData;
  Point3D accZero;
  std::deque<LogEntry<Point3D>> accLog;
};

static AccelerationLog accelLog([](float *x, float *y, float *z)
                                { M5.Imu.getAccelData(x, y, z); });

void drawInformation()
{
  sprite.clear();
  sprite.setTextColor(WHITE, BLACK);
  sprite.setTextSize(1);
  sprite.setFont(&AsciiFont8x16);
  sprite.setCursor(0, 0);
  sprite.println(" Acceleration");
  sprite.println(" ----------------");

  for (auto it = accelLog.begin(); it != accelLog.end(); ++it)
  {
    const auto &logEntry = *it;
    sprite.print(" X:");
    sprite.print(logEntry.data.x);
    sprite.print(", Y:");
    sprite.print(logEntry.data.y);
    sprite.print(", Z:");
    sprite.print(logEntry.data.z);
    sprite.print(", T:");
    sprite.println(logEntry.timestamp);
  }

  sprite.pushSprite(0, 0);
}

void resetAcceleration()
{
  accelLog.reset();
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

  accelLog.init();

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
  // drawGraph();
  delay(250);
}
