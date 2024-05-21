#include <deque>
#include <functional>

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

static const float max_abs_accel = 0.5F;

static const float major_scale_frequencies[] = {
    // 130.81, // C3
    // 146.83, // D3
    // 164.81, // E3
    // 174.61, // F3
    // 196.00, // G3
    // 220.00, // A3
    // 246.94, // B3
    261.63, // C4
    // 293.66, // D4
    329.63, // E4
    // 349.23, // F4
    392.00, // G4
    // 440.00, // A4
    // 493.88, // B4
    523.25, // C5
    // 587.33, // D5
    659.26, // E5
    // 698.46, // F5
    783.99, // G5
            // 880.00, // A5
            // 987.77  // B5

};
static const auto major_scale_frequencies_len = sizeof(major_scale_frequencies) / sizeof(major_scale_frequencies[0]);

struct RGB
{
  uint8_t r;
  uint8_t g;
  uint8_t b;

  RGB &operator/=(int divisor)
  {
    r /= divisor;
    g /= divisor;
    b /= divisor;
    return *this;
  }

  RGB operator/(int divisor) const
  {
    return RGB{static_cast<uint8_t>(r / divisor), static_cast<uint8_t>(g / divisor), static_cast<uint8_t>(b / divisor)};
  }

  RGB operator*(int multiplier) const
  {
    return RGB{static_cast<uint8_t>(r * multiplier), static_cast<uint8_t>(g * multiplier), static_cast<uint8_t>(b * multiplier)};
  }

  RGB operator-(const RGB &rop) const
  {
    return RGB{static_cast<uint8_t>(r - min(r, rop.r)), static_cast<uint8_t>(g - min(g, rop.g)), static_cast<uint8_t>(b - min(b, rop.b))};
  }

  // implicit catst to uint32_t
  operator uint32_t() const
  {
    return (r << 16) | (g << 8) | b;
  }

  uint32_t raw_color() const
  {
    return *this;
  }
};

struct Point3D
{
  float x;
  float y;
  float z;

  Point3D &operator-=(const Point3D &rhs)
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

  float acc_x() const
  {
    return sqrt(z * z + y * y) * -(y >= 0 ? 1 : -1);
  }

  float acc_y() const
  {
    return sqrt(z * z + x * x) * -(x >= 0 ? 1 : -1);
  }

  float acc_magnitude() const
  {
    return sqrt(x * x + y * y + z * z);
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
  const int max_log_size;

  AccelerationLog(std::function<void(float *, float *, float *)> getAccelDataFunc, int max_log_size = 50)
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

  std::deque<LogEntry<Point3D>>::const_iterator begin() const
  {
    return accLog.begin();
  }

  std::deque<LogEntry<Point3D>>::const_iterator end() const
  {
    return accLog.end();
  }

  std::deque<LogEntry<Point3D>>::const_reverse_iterator rbegin() const
  {
    return accLog.rbegin();
  }

  std::deque<LogEntry<Point3D>>::const_reverse_iterator rend() const
  {
    return accLog.rend();
  }

  const LogEntry<Point3D> &head() const
  {
    return accLog.back();
  }

  bool empty() const
  {
    return accLog.empty();
  }

private:
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

  for (auto it = accelLog.rbegin(); it != accelLog.rend(); ++it)
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

void drawGraph()
{
  sprite.clear();

  const auto min_grid_count = static_cast<int>(max_abs_accel * 10);
  const auto grid_length = min(sprite.width(), sprite.height()) / (min_grid_count * 2);
  const auto max_grid_count = max(sprite.width(), sprite.height()) / (grid_length * 2) + 1;
  // grid_length = 0.1 G
  const RGB grid_color = {0x80, 0x80, 0x80};
  const auto centor_x = sprite.width() / 2;
  const auto centor_y = sprite.height() / 2;

  const auto grid_color_diff = grid_color / max_grid_count;
  for (int i = max_grid_count; i > 0; i -= 1)
  {
    sprite.fillCircle(centor_x, centor_y, i * grid_length, (grid_color - grid_color_diff * (grid_length - i)).raw_color());
    sprite.drawCircle(centor_x, centor_y, i * grid_length, grid_color.raw_color());
  }

  // draw cross
  sprite.drawLine(0, centor_y, sprite.width(), centor_y, grid_color.raw_color());
  sprite.drawLine(centor_x, 0, centor_x, sprite.height(), grid_color.raw_color());

  // draw acceleration
  const int max_log_size = accelLog.max_log_size;
  RGB entry_color = {0x08, 0x08, 0x08};
  const uint8_t color_diff = 0xFF / max_log_size;
  for (auto it = accelLog.begin(); it != accelLog.end(); ++it)
  {
    const auto &logEntry = *it;
    sprite.setColor(entry_color.r, entry_color.g, entry_color.b);
    sprite.fillCircle(logEntry.data.acc_x() * 10.0f * grid_length + centor_x, logEntry.data.acc_y() * 10.0f * grid_length + centor_y, 2);

    // r += diff
    entry_color.r = min(0xFF, entry_color.r + color_diff);
  }

  const auto &data = accelLog.head().data;
  sprite.setTextColor(WHITE, BLACK);
  sprite.setTextSize(1);
  sprite.setFont(&AsciiFont8x16);
  sprite.setCursor(8, 8);
  sprite.print("X:");
  sprite.print(data.x);
  sprite.print(", Y:");
  sprite.print(data.y);
  sprite.print(", Z:");
  sprite.println(data.z);

  sprite.pushSprite(0, 0);
}

static bool use_beep = true;
static uint8_t last_beep_vol = 0;
static uint16_t last_beep_freq = 0;

void updateBeep()
{
  if (!use_beep || accelLog.empty())
  {
    M5.Beep.mute();
    last_beep_vol = 0;
    last_beep_freq = 0;
    return;
  }

  const auto mag_min = 0.2f;

  const auto &head = accelLog.head();
  const auto mag = min(max_abs_accel, head.data.acc_magnitude());
  if (mag < mag_min)
  {
    M5.Beep.mute();
    last_beep_vol = 0;
    last_beep_freq = 0;
    return;
  }

  const uint8_t vol = static_cast<uint8_t>(mag / max_abs_accel * 10);
  const int scale = static_cast<int>((mag - mag_min) * 10);
  const uint16_t freq = major_scale_frequencies[min(major_scale_frequencies_len - 1, scale)];
  if (last_beep_vol == vol && last_beep_freq == freq)
  {
    return;
  }

  M5.Beep.setVolume(vol);
  last_beep_vol = vol;

  M5.Beep.tone(freq);
  last_beep_freq = freq;

  debug_printf("Beep: mag:%0.4f vol:%02d freq:%04d, scale:%02d\n", (double)mag, (int)vol, (int)freq, scale);
}

void setupSprite()
{
  sprite.createSprite(lcd.width(), lcd.height()); // Create a sprite with width 80 and height 160

  sprite.fillSprite(BLACK);

  sprite.setTextColor(WHITE, BLACK);
  sprite.setTextSize(2);
  sprite.setFont(&AsciiFont8x16);
  sprite.setCursor(16, 16);
  sprite.print("Sprite Ready");
  debug_println("Sprite Ready");
  sprite.pushSprite(0, 0); // Push the sprite to the display at position (0, 0)
}

void setup()
{
  M5.begin();

  // Beep
  M5.Beep.begin();
  M5.Beep.setVolume(5);

  debug_begin(115200);

  M5.IMU.Init();

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
  accelLog.update();

  if (M5.BtnA.pressedFor(1500))
  {
    accelLog.reset();
  }
  else if (M5.BtnA.wasPressed())
  {
  }

  if (M5.BtnB.wasPressed())
  {
    use_beep = !use_beep;
    M5.Beep.setVolume(8);
    if (use_beep)
    {
      M5.Beep.tone(1000, 250);
    }
    else
    {
      // twice
      M5.Beep.tone(1000, 250);
      delay(250 * 125);
      M5.Beep.tone(1000, 250);
    }
  }
  // drawInformation();
  drawGraph();
  updateBeep();
  delay(100);
}
