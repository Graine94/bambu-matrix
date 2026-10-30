// Статус печати Bambu Lab на матрице MAX7219 64x8 (ESP32).
// Принтер отдаёт статус по MQTT поверх TLS (порт 8883, логин bblp, пароль = Access Code).
// Серийный номер принтера (нужен для MQTT-топика) берётся из его TLS-сертификата,
// принтеры в сети находятся сами по SSDP.
//
// Веб-морда: http://bambu-matrix.local (или IP, который матрица прокручивает после
// подключения к Wi-Fi) — вид прогресса, яркость, таймауты, принтер.
// Прошивка по Wi-Fi: pio run -e ota -t upload, или «Обновить прошивку» в веб-морде.
// Wi-Fi: http://bambu-matrix.local/wifi, или точка доступа BambuMatrix / bambu123,
// которая поднимается сама, если сети нет дольше WIFI_GIVE_UP (или GPIO0 на GND 3 секунды).

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>

#include "web_page.h"
#include "github_ca.h"

// Версию подставляет scripts/version.py из тега git (v1.2.0); без PlatformIO — "dev"
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
// Откуда брать обновления. В форке поменяй на свой репозиторий
#define UPDATE_REPO  "Graine94/bambu-matrix"
#define UPDATE_ASSET "bambu-matrix-ota.bin"

// ---------- Матрица ----------
// Аппаратный SPI (VSPI): DIN -> GPIO23, CLK -> GPIO18, CS -> GPIO5
// Если картинка «в кашу», поменяй тип: PAROLA_HW, GENERIC_HW, ICSTATION_HW.
#define HW_TYPE      MD_MAX72XX::FC16_HW
#define CS_PIN       5
#define MODULES      8
#define WIDTH        (MODULES * 8)

MD_MAX72XX mx(HW_TYPE, CS_PIN, MODULES);
uint8_t fb[WIDTH];  // fb[x] — столбец x слева направо, бит 0 — верхняя строка

// ---------- Настройки ----------
#define AP_NAME      "BambuMatrix"
#define AP_PASS      "bambu123"
#define OTA_PASS     "bambu123"
#define HOSTNAME     "bambu-matrix"
#define BUTTON_PIN   0   // на LOLIN32 Lite кнопки нет, можно замкнуть GPIO0 на GND
#define CONFIG_FILE  "/config.json"

enum IdleMode { IDLE_OFF = 0, IDLE_CLOCK = 1, IDLE_TEXT = 2, IDLE_RAIN = 3, IDLE_CLOCK_WEATHER = 4 };
const int TZ_AUTO = 9999;  // часовой пояс берётся из погоды по выбранному городу

struct Config {
  char ip[40] = "";
  char code[16] = "";
  bool verified = false;  // с этими IP и кодом уже хоть раз подключились — значит, тишина = принтер выключен
  int moduleType = 1;     // MD_MAX72XX::moduleType_t: 0 GENERIC, 1 FC16, 2 PAROLA, 3 ICSTATION
  char lang[3] = "en";    // язык веб-морды: en или ru
  int style = 3;          // вид прогресса: 1, 3..7 (2 убран)
  int brightness = 2;     // 0..15, днём (или всегда, если ночной режим выключен)
  int nightMode = 0;      // 0 — выкл, 1 — по расписанию, 2 — от заката до рассвета
  int nightBrightness = 0;
  int nightStart = 22 * 60;  // минуты от полуночи
  int nightEnd = 7 * 60;
  int doneTimeout = 10;   // минут показывать DONE/FAIL, 0 — не убирать
  int idleMode = IDLE_OFF;  // что показывать, когда принтер не печатает
  int tzOffset = 180;     // часовой пояс для часов, минуты от UTC, или TZ_AUTO
  bool clockBlink = true; // мигающее двоеточие в часах
  char city[48] = "";    // город для погоды (пусто — погода выключена)
  float lat = 0, lon = 0;
  int cityTz = TZ_AUTO;   // сдвиг города от UTC в минутах (считает браузер) — для met.no
  char serial[24] = "";  // серийник вручную, если не читается из сертификата
  bool flip = false;      // повернуть на 180°
  bool heatView = true;   // при подготовке показывать нагрев сопла и стола
  bool doneAnim = true;   // галочка и искры в первые секунды после окончания
  int pauseMode = 0;      // 0 — мигает весь экран, 1 — мигает полоса, 2 — надпись PAUSE
  bool coolView = true;   // после печати ждать остывания стола: COOL, потом READY
  int coolTarget = 35;    // до какой температуры стола ждать
  bool fade = true;       // плавное появление и исчезновение экрана
} cfg;

const uint32_t DONE_ANIM_LEN = 10000;

String printerSerial;  // из сертификата принтера или из настроек

// Форма WiFiManager (точка доступа BambuMatrix): Wi-Fi обязательно, принтер — можно позже
WiFiManager wm;
WiFiManagerParameter pHint(
    "<hr><p><b>Bambu Lab printer</b> &mdash; optional here. You can set it up later on the matrix page: "
    "once on Wi-Fi the matrix scrolls its address, and printers on the network are found automatically.</p>"
    "<p>The IP and Access Code are on the printer screen in the network (WLAN) settings.</p>");
WiFiManagerParameter pIp("ip", "Printer IP", "", sizeof(cfg.ip) - 1);
WiFiManagerParameter pCode("code", "Access Code", "", sizeof(cfg.code) - 1);

// ---------- Сеть ----------
WiFiClientSecure net;
MqttClient mqtt(net);
uint32_t lastMqttTry = 0;
uint32_t mqttRetry = 5000;  // растёт до 30 с, пока принтер выключен
uint32_t lastWifiTry = 0;
uint32_t wifiLostSince = 0;  // 0 — Wi-Fi подключён
const uint32_t WIFI_RETRY = 20000;          // как часто переподключаться
const uint32_t WIFI_GIVE_UP = 3 * 60000;    // через сколько без сети открыть SETUP
bool manualPortal = false;  // SETUP открыт вручную — не закрывать, даже если Wi-Fi есть
bool mdnsStarted = false;

// ---------- Состояние принтера ----------
String gcodeState = "";   // IDLE, PREPARE, RUNNING, PAUSE, FINISH, FAILED
int percent = -1;
int remaining = -1;       // минут до конца
int layer = -1, totalLayers = -1;
float nozzleTemp = -1, nozzleTarget = 0, bedTemp = -1, bedTarget = 0;
bool haveData = false;
uint32_t stateSince = 0;  // когда gcodeState последний раз менялся
float bedAtFinish = 0;    // температура стола в момент окончания печати
uint32_t coolDoneAt = 0;  // когда стол остыл до coolTarget (0 — ещё горячий)
bool displayOff = false;
uint32_t demoStart = 0;   // демо-прогон из веб-морды
String demoType = "progress";  // progress, heat, done, pause, cool, rain
const uint32_t DEMO_LEN = 15000;

// Отрисовка матрицы крутится в отдельной задаче на ядре 0, а MQTT, веб-морда
// и Wi-Fi-логика — в loop() на ядре 1. Всё, что читают обе стороны
// (состояние принтера, настройки, кадр), меняется только под этим мьютексом.
SemaphoreHandle_t stateLock;
struct Lock {
  Lock() { xSemaphoreTake(stateLock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(stateLock); }
};

volatile bool printerOnline = false;  // mqtt.connected() с ядра 1 — сам клиент трогать с ядра 0 нельзя
volatile bool otaActive = false;
volatile size_t otaDone = 0, otaTotal = 0;
volatile uint32_t otaLast = 0;  // если прогресса нет 10 с — загрузка сорвалась

bool configured() { return cfg.ip[0] && cfg.code[0]; }
bool demoActive() { return demoStart && millis() - demoStart < DEMO_LEN; }

// ---------- Хранение настроек ----------
void configToJson(JsonDocument &doc) {
  doc["ip"] = cfg.ip;
  doc["code"] = cfg.code;
  doc["style"] = cfg.style;
  doc["brightness"] = cfg.brightness;
  doc["nightMode"] = cfg.nightMode;
  doc["nightBrightness"] = cfg.nightBrightness;
  doc["nightStart"] = cfg.nightStart;
  doc["nightEnd"] = cfg.nightEnd;
  doc["doneTimeout"] = cfg.doneTimeout;
  doc["idleMode"] = cfg.idleMode;
  doc["tzOffset"] = cfg.tzOffset;
  doc["flip"] = cfg.flip;
  doc["heatView"] = cfg.heatView;
  doc["doneAnim"] = cfg.doneAnim;
  doc["pauseMode"] = cfg.pauseMode;
  doc["coolView"] = cfg.coolView;
  doc["coolTarget"] = cfg.coolTarget;
  doc["fade"] = cfg.fade;
  doc["clockBlink"] = cfg.clockBlink;
  doc["city"] = cfg.city;
  doc["lat"] = cfg.lat;
  doc["lon"] = cfg.lon;
  doc["cityTz"] = cfg.cityTz;
  doc["serial"] = cfg.serial;
  doc["verified"] = cfg.verified;
  doc["moduleType"] = cfg.moduleType;
  doc["lang"] = cfg.lang;
}

void configFromJson(JsonDocument &doc) {
  if (doc["ip"].is<const char *>()) strlcpy(cfg.ip, doc["ip"], sizeof(cfg.ip));
  if (doc["code"].is<const char *>()) strlcpy(cfg.code, doc["code"], sizeof(cfg.code));
  cfg.style = doc["style"] | cfg.style;
  if (cfg.style < 1 || cfg.style == 2 || cfg.style > 7) cfg.style = 3;  // виды 2, 8, 9 убраны
  cfg.brightness = constrain(doc["brightness"] | cfg.brightness, 0, 15);
  cfg.nightMode = constrain(doc["nightMode"] | cfg.nightMode, 0, 2);
  cfg.nightBrightness = constrain(doc["nightBrightness"] | cfg.nightBrightness, 0, 15);
  cfg.nightStart = constrain(doc["nightStart"] | cfg.nightStart, 0, 1439);
  cfg.nightEnd = constrain(doc["nightEnd"] | cfg.nightEnd, 0, 1439);
  cfg.doneTimeout = constrain(doc["doneTimeout"] | cfg.doneTimeout, 0, 1440);
  cfg.idleMode = constrain(doc["idleMode"] | cfg.idleMode, 0, 4);
  if (cfg.idleMode == IDLE_TEXT) cfg.idleMode = IDLE_CLOCK;  // надпись idle убрана из веб-морды
  int tz = doc["tzOffset"] | cfg.tzOffset;
  cfg.tzOffset = tz == TZ_AUTO ? TZ_AUTO : constrain(tz, -720, 840);
  cfg.flip = doc["flip"] | cfg.flip;
  cfg.heatView = doc["heatView"] | cfg.heatView;
  cfg.doneAnim = doc["doneAnim"] | cfg.doneAnim;
  cfg.pauseMode = constrain(doc["pauseMode"] | cfg.pauseMode, 0, 2);
  cfg.coolView = doc["coolView"] | cfg.coolView;
  cfg.coolTarget = constrain(doc["coolTarget"] | cfg.coolTarget, 20, 60);
  cfg.fade = doc["fade"] | cfg.fade;
  cfg.clockBlink = doc["clockBlink"] | cfg.clockBlink;
  if (doc["city"].is<const char *>()) strlcpy(cfg.city, doc["city"], sizeof(cfg.city));
  cfg.lat = doc["lat"] | cfg.lat;
  cfg.lon = doc["lon"] | cfg.lon;
  cfg.cityTz = doc["cityTz"] | cfg.cityTz;
  if (doc["serial"].is<const char *>()) strlcpy(cfg.serial, doc["serial"], sizeof(cfg.serial));
  cfg.verified = doc["verified"] | cfg.verified;
  cfg.moduleType = constrain(doc["moduleType"] | cfg.moduleType, 0, 3);
  const char *lang = doc["lang"] | cfg.lang;
  strlcpy(cfg.lang, strcmp(lang, "ru") == 0 ? "ru" : "en", sizeof(cfg.lang));
}

void loadConfig() {
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;
  JsonDocument doc;
  if (!deserializeJson(doc, f)) configFromJson(doc);
  f.close();
}

void saveConfig() {
  JsonDocument doc;
  configToJson(doc);
  File f = LittleFS.open(CONFIG_FILE, "w");
  serializeJson(doc, f);
  f.close();
}

// ---------- Погода (Open-Meteo: без ключа и регистрации) ----------
struct Weather {
  float temp = 0;
  int code = -1;          // код погоды WMO
  bool day = true;
  long utcOffset = 0;     // секунды, для автоматического часового пояса
  uint32_t updated = 0;   // millis() последнего успешного обновления, 0 — ещё нет
  int sunrise = -1, sunset = -1;  // минуты от полуночи по местному времени города
  int source = 0;         // откуда последняя погода: 1 — Open-Meteo, 2 — met.no
  bool failed = false;    // последняя попытка: оба источника недоступны
  String err;             // что пошло не так в последний раз — для /api/state
} weather;
uint32_t lastWeatherTry = 0;
const uint32_t WEATHER_EVERY = 15 * 60000;
const uint32_t WEATHER_RETRY = 60000;
const uint32_t WEATHER_STALE = 2 * 3600000;  // старше — не показываем

bool weatherFresh() { return weather.updated && millis() - weather.updated < WEATHER_STALE; }

// Минуты из "2026-10-27T07:12" (Open-Meteo отдаёт местное время города)
int minutesFromIso(const char *s) {
  if (!s || strlen(s) < 16) return -1;
  return atoi(s + 11) * 60 + atoi(s + 14);
}

// Местное время, если часы уже синхронизированы. Не getLocalTime(&tm, 0): там цикл
// «пока прошло <= 0 мс», и если millis() успел смениться, он сразу возвращает false —
// раз в несколько секунд часы на кадр превращались в «--:--» (матрица «моргала»).
bool localNow(struct tm &tm) {
  time_t now = time(nullptr);
  localtime_r(&now, &tm);
  return tm.tm_year > 2016 - 1900;
}

// Ночь ли сейчас — для ночной яркости. Без синхронизированных часов считаем, что день.
bool isNight() {
  if (cfg.nightMode == 0) return false;
  struct tm tm;
  if (!localNow(tm)) return false;
  int now = tm.tm_hour * 60 + tm.tm_min;
  int from = cfg.nightStart, to = cfg.nightEnd;
  // По солнцу — если погода уже знает восход и закат, иначе по расписанию
  if (cfg.nightMode == 2 && weather.sunrise >= 0 && weather.sunset >= 0) {
    from = weather.sunset;
    to = weather.sunrise;
  }
  if (from == to) return false;
  return from < to ? (now >= from && now < to) : (now >= from || now < to);
}

void applyTimezone() {
  long offset = cfg.tzOffset == TZ_AUTO ? weather.utcOffset : cfg.tzOffset * 60L;
  configTime(offset, 0, "pool.ntp.org", "time.google.com");
}

// Почему не удалось подключиться к принтеру — показываем на матрице, пока
// с этими настройками ещё ни разу не подключались (cfg.verified == false)
enum PrinterError { PERR_NONE, PERR_NOT_FOUND, PERR_CODE, PERR_SERIAL };
volatile int printerError = PERR_NONE;

// Новые IP/код/серийник: всё заново, включая проверку
void reconnectPrinter() {
  mqtt.stop();
  haveData = false;
  lastMqttTry = 0;
  mqttRetry = 0;
  printerError = PERR_NONE;
  cfg.verified = false;
}

// Вызывается, когда в форме WiFiManager нажали Save
void onParamsSaved() {
  Lock lock;
  if (strcmp(cfg.ip, pIp.getValue()) == 0 && strcmp(cfg.code, pCode.getValue()) == 0) return;
  strlcpy(cfg.ip, pIp.getValue(), sizeof(cfg.ip));
  strlcpy(cfg.code, pCode.getValue(), sizeof(cfg.code));
  reconnectPrinter();
  saveConfig();
  Serial.printf("Config saved: ip=%s\n", cfg.ip);
}

// ---------- Рисование ----------
void fbClear() { memset(fb, 0, sizeof(fb)); }

uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  return (b & 0xAA) >> 1 | (b & 0x55) << 1;
}

uint8_t shown[WIDTH];  // что сейчас реально на матрице (с учётом растворения) — для веб-морды

void flush(const uint8_t *buf) {
  memcpy(shown, buf, WIDTH);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  // В MD_MAX72XX столбец 0 — крайний правый
  for (int x = 0; x < WIDTH; x++) {
    if (cfg.flip) mx.setColumn(x, reverseBits(buf[x]));
    else mx.setColumn(WIDTH - 1 - x, buf[x]);
  }
  mx.update();
}

void setPixel(int x, int y) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < 8) fb[x] |= 1 << y;
}

void fillRect(int x0, int y0, int x1, int y1) {
  for (int x = x0; x <= x1; x++)
    for (int y = y0; y <= y1; y++) setPixel(x, y);
}

// Рамка во всю высоту от x0 до x1
void drawFrame(int x0, int x1) {
  for (int x = x0; x <= x1; x++) fb[x] |= 0x81;
  fb[x0] = fb[x1] = 0xFF;
}

// Основной шрифт 5x7 из MD_MAX72XX
int textWidth(const char *s) {
  uint8_t buf[8];
  int w = 0;
  for (; *s; s++) w += mx.getChar(*s, sizeof(buf), buf) + 1;
  return w > 0 ? w - 1 : 0;
}

int drawText(int x, const char *s, bool invert = false) {
  uint8_t buf[8];
  for (; *s; s++) {
    uint8_t w = mx.getChar(*s, sizeof(buf), buf);
    for (int i = 0; i < w; i++)
      if (x + i >= 0 && x + i < WIDTH) fb[x + i] = invert ? fb[x + i] ^ buf[i] : fb[x + i] | buf[i];
    x += w + 1;
  }
  return x;
}

void drawCentered(const char *s, bool invert = false) {
  drawText((WIDTH - textWidth(s)) / 2, s, invert);
}

// Бегущая строка справа налево; since — когда начали, чтобы каждый текст шёл с начала
const uint32_t SCROLL_MS = 35;  // мс на точку
uint32_t scrollPassMs(const char *s) { return (textWidth(s) + WIDTH) * SCROLL_MS; }

void drawScroll(const char *s, uint32_t since) {
  int w = textWidth(s) + WIDTH;
  int offset = ((millis() - since) / SCROLL_MS) % w;
  drawText(WIDTH - offset, s);
}

// Мелкий шрифт 3x5 — цифры, %, двоеточие, минус
const char *glyph3(char c) {
  switch (c) {
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "111001111100111";
    case '3': return "111001111001111";
    case '4': return "101101111001001";
    case '5': return "111100111001111";
    case '6': return "111100111101111";
    case '7': return "111001001001001";
    case '8': return "111101111101111";
    case '9': return "111101111001111";
    case '-': return "000000111000000";
    case '+': return "000010111010000";
    default:  return nullptr;
  }
}

int drawIcon(int x, int y, const char *const rows[], int h);

// Знак % 5x5 к мелкому шрифту — того же рисунка, что в основном шрифте: точки 2x2 и черта
const char *const SMALL_PCT[] = {"11001", "11010", "00100", "01011", "10011"};

int smallWidth(const char *s) {
  int w = 0;
  for (; *s; s++) w += (*s == ':' ? 1 : *s == '%' ? 5 : 3) + 1;
  return w > 0 ? w - 1 : 0;
}

int drawSmall(int x, int y, const char *s) {
  for (; *s; s++) {
    if (*s == ':') {
      setPixel(x, y + 1);
      setPixel(x, y + 3);
      x += 2;
      continue;
    }
    if (*s == '%') {
      x = drawIcon(x, y, SMALL_PCT, 5) + 1;
      continue;
    }
    const char *g = glyph3(*s);
    if (g)
      for (int i = 0; i < 15; i++)
        if (g[i] == '1') setPixel(x + i % 3, y + i / 3);
    x += 4;
  }
  return x;
}

// Средний шрифт 3x6 — цифры, %, двоеточие; по высоте как сегменты и рамка полосы
const char *glyph6(char c) {
  switch (c) {
    case '0': return "111101101101101111";
    case '1': return "010110010010010111";
    case '2': return "111001001111100111";
    case '3': return "111001111001001111";
    case '4': return "101101101111001001";
    case '5': return "111100111001001111";
    case '6': return "111100111101101111";
    case '7': return "111001001001001001";
    case '8': return "111101111101101111";
    case '9': return "111101101111001111";
    default:  return nullptr;
  }
}

const char *const MID_PCT[] = {"110001", "110010", "000100", "001000", "010011", "100011"};

int midWidth(const char *s) {
  int w = 0;
  for (; *s; s++) w += (*s == ':' ? 1 : *s == '%' ? 6 : 3) + 1;
  return w > 0 ? w - 1 : 0;
}

int drawMid(int x, int y, const char *s) {
  for (; *s; s++) {
    if (*s == ':') {
      setPixel(x, y + 1);
      setPixel(x, y + 4);
      x += 2;
    } else if (*s == '%') {
      x = drawIcon(x, y, MID_PCT, 6) + 1;
    } else {
      const char *g = glyph6(*s);
      if (g)
        for (int i = 0; i < 18; i++)
          if (g[i] == '1') setPixel(x + i % 3, y + i / 3);
      x += 4;
    }
  }
  return x;
}

int scaled(int pct, int n) { return (constrain(pct, 0, 100) * n + 50) / 100; }

void formatTime(char *out, size_t n, int minutes) {
  if (minutes < 0) snprintf(out, n, "-:--");
  else snprintf(out, n, "%d:%02d", minutes / 60, minutes % 60);
}

// ---------- Виды прогресса (см. веб-морду) ----------
// pct — число на экране, barPct — заливка полосы (на паузе полоса может мигать отдельно)
void drawProgress(int style, int pct, int remMin, bool blink, int barPct) {
  char p[6], t[8];
  snprintf(p, sizeof(p), "%d%%", constrain(pct, 0, 100));
  formatTime(t, sizeof(t), remMin);

  switch (style) {
    case 1: {  // процент слева, полоса в рамке; рамка начинается за местом под «100%»
      drawMid(0, 1, p);
      drawFrame(19, 63);
      int k = scaled(barPct, 43);
      if (k) fillRect(20, 2, 19 + k, 5);
      break;
    }
    case 3: {  // мелкий процент и время сверху, полоса 2 строки снизу
      drawSmall(0, 0, p);
      drawSmall(WIDTH - smallWidth(t), 0, t);
      int k = scaled(barPct, WIDTH);
      if (k) fillRect(0, 6, k - 1, 7);
      break;
    }
    case 4: {  // 10 сегментов, текущий мигает
      drawMid(0, 1, p);
      int full = constrain(barPct, 0, 100) / 10;
      for (int i = 0; i < 10; i++) {
        int x = 15 + i * 5;
        if (i < full || (i == full && barPct < 100 && blink)) {
          fillRect(x, 1, x + 3, 6);
        } else {
          setPixel(x, 1); setPixel(x + 3, 1);
          setPixel(x, 6); setPixel(x + 3, 6);
        }
      }
      break;
    }
    case 5: {  // вся матрица, колонки снизу вверх
      int k = scaled(barPct, WIDTH * 8);
      for (int i = 0; i < k; i++) setPixel(i / 8, 7 - i % 8);
      break;
    }
    case 6: {  // процент, полоса, время в одну строку
      drawMid(0, 1, p);
      drawMid(WIDTH - midWidth(t), 1, t);
      drawFrame(16, 46);
      int k = scaled(barPct, 29);
      if (k) fillRect(17, 2, 16 + k, 5);
      break;
    }
    default: {  // 7: крупный процент и линия снизу
      drawCentered(p);
      int k = scaled(barPct, WIDTH);
      if (k) fillRect(0, 7, k - 1, 7);
      break;
    }
  }
}

// Бегущий блок в рамке — пока принтер готовится
void drawPrepare() {
  drawText(0, "prep");
  drawFrame(25, 63);
  const int inner = 37;
  int pos = (millis() / 60) % inner;
  for (int i = 0; i < 4; i++) fb[26 + (pos + i) % inner] |= 0x3C;
}

// Иконка из строк '0'/'1', возвращает x правого края
int drawIcon(int x, int y, const char *const rows[], int h) {
  int w = strlen(rows[0]);
  for (int ry = 0; ry < h; ry++)
    for (int rx = 0; rx < w; rx++)
      if (rows[ry][rx] == '1') setPixel(x + rx, y + ry);
  return x + w;
}

const char *const ICON_NOZZLE[] = {"11111", "11111", "01110", "01110", "00100"};
const char *const ICON_BED[] = {"1111111", "1000001"};
const char *const ICON_CHECK[] = {"0000001", "0000010", "1000100", "0101000", "0010000"};

// Принтер ещё греется до целевых температур
bool heating() {
  bool nozzle = nozzleTarget > 0 && nozzleTemp < nozzleTarget - 3;
  bool bed = bedTarget > 0 && bedTemp < bedTarget - 2;
  return nozzle || bed;
}

int heatPct(float cur, float target) {
  if (target <= 0) return 0;
  const float ambient = 25;
  if (target <= ambient) return 100;
  return constrain((int)((cur - ambient) * 100 / (target - ambient)), 0, 100);
}

// Сопло и стол: температура и полоска до целевой
void drawHeating(float nozzle, float nozzleTgt, float bed, float bedTgt) {
  char t[6];
  snprintf(t, sizeof(t), "%d", (int)roundf(max(nozzle, 0.0f)));
  drawSmall(drawIcon(0, 0, ICON_NOZZLE, 5) + 2, 0, t);
  snprintf(t, sizeof(t), "%d", (int)roundf(max(bed, 0.0f)));
  drawIcon(33, 2, ICON_BED, 2);
  drawSmall(42, 0, t);

  int kn = scaled(heatPct(nozzle, nozzleTgt), 29);
  int kb = scaled(heatPct(bed, bedTgt), 30);
  if (kn) fillRect(0, 6, kn - 1, 7);
  if (kb) fillRect(33, 6, 32 + kb, 7);
}

// Галочка, DONE и искры по краям
void drawDoneAnim() {
  int x = drawIcon(10, 1, ICON_CHECK, 5) + 4;
  drawText(x, "DONE");
  for (int i = 0; i < 6; i++) {
    uint32_t r = esp_random();
    int sx = r % 18;
    setPixel(sx < 9 ? sx : 46 + sx, (r >> 8) % 8);
  }
}

void drawPause(bool blink, int pct, int rem) {
  bool phase = (millis() / 700) % 2;
  switch (cfg.pauseMode) {
    case 1:  // мигает только полоса
      drawProgress(cfg.style, pct, rem, blink, phase ? pct : 0);
      break;
    case 2: {  // надпись PAUSE и процент
      char p[6];
      snprintf(p, sizeof(p), "%d%%", constrain(pct, 0, 100));
      drawText(0, "PAUSE");
      drawText(WIDTH - textWidth(p), p);
      break;
    }
    default:  // мигает весь экран
      if (phase) drawProgress(cfg.style, pct, rem, blink, pct);
  }
}

// Заставка «дождь»: капли длиной 2 точки падают с разной скоростью
struct Drop { int8_t x; float y, v; };
Drop drops[14];

void resetDrop(Drop &d, bool anywhere) {
  d.x = esp_random() % WIDTH;
  d.y = anywhere ? (int)(esp_random() % 10) - 1.0f : -1.0f;
  d.v = 0.15f + (esp_random() % 100) / 400.0f;
}

void drawRain() {
  static bool init = false;
  if (!init) {
    for (auto &d : drops) resetDrop(d, true);
    init = true;
  }
  for (auto &d : drops) {
    d.y += d.v;
    if (d.y > 9) resetDrop(d, false);
    setPixel(d.x, (int)d.y);
    setPixel(d.x, (int)d.y - 1);
  }
}

// Стол остывает: COOL, температура и полоса от температуры в конце печати до цели
void drawCooling(float bed, float from, int target) {
  int x = drawText(0, "COOL") + 4;
  drawIcon(x, 2, ICON_BED, 2);
  char t[6];
  snprintf(t, sizeof(t), "%d", (int)roundf(bed));
  int tx = drawSmall(x + 10, 0, t);
  fillRect(tx, 0, tx + 1, 1);  // значок градуса
  float span = max(from - target, 1.0f);
  int k = scaled((int)((from - bed) * 100 / span), WIDTH);
  if (k) fillRect(0, 7, k - 1, 7);
}

void drawReady() {
  int x = drawIcon(6, 1, ICON_CHECK, 5) + 4;
  drawText(x, "READY");
}

// Иконки погоды высотой 6 точек по коду WMO — рисуются со 2-й строки, по центру высоты
const char *const W_SUN[] = {"01000010", "00011000", "10111101", "10111101", "00011000", "01000010"};
const char *const W_MOON[] = {"0110010", "1100111", "1000010", "1000000", "1100000", "0110000"};
const char *const W_PARTLY_DAY[] = {"00001010", "00010110", "01101111", "10010110", "10000001", "01111110"};
const char *const W_PARTLY_NIGHT[] = {"00000110", "00110011", "01001010", "10000100", "10000001", "01111110"};
const char *const W_CLOUD[] = {"00011000", "00100110", "01000001", "10000001", "10000001", "01111110"};
const char *const W_FOG[] = {"11111100", "00000000", "01111110", "00000000", "11111100", "00000000"};
const char *const W_DRIZZLE[] = {"00011000", "01100110", "10000001", "01111110", "00100010", "00001000"};
const char *const W_RAIN[] = {"00011000", "01100110", "10000001", "01111110", "00100100", "01001000"};
const char *const W_SNOW[] = {"00011000", "01100110", "10000001", "01111110", "01001001", "10010010"};
const char *const W_STORM[] = {"00011000", "01100110", "10000001", "01110110", "00011000", "00001000"};

const char *const *weatherIcon(int code, bool day) {
  if (code == 0) return day ? W_SUN : W_MOON;
  if (code <= 2) return day ? W_PARTLY_DAY : W_PARTLY_NIGHT;
  if (code == 3) return W_CLOUD;
  if (code == 45 || code == 48) return W_FOG;
  if (code >= 51 && code <= 57) return W_DRIZZLE;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return W_SNOW;
  if (code >= 95) return W_STORM;
  return W_RAIN;  // дождь, ливни
}

// Цифры часов 4x8 на всю высоту, по столбцам (бит 0 — верхняя строка)
const uint8_t DIGIT8[10][4] = {
  {0x7E, 0x81, 0x81, 0x7E}, {0x00, 0x82, 0xFF, 0x80}, {0xC2, 0xA1, 0x91, 0x8E}, {0x42, 0x89, 0x89, 0x76},
  {0x1F, 0x10, 0x10, 0xFF}, {0x4F, 0x89, 0x89, 0x71}, {0x7E, 0x89, 0x89, 0x70}, {0x01, 0xE1, 0x19, 0x07},
  {0x76, 0x89, 0x89, 0x76}, {0x0E, 0x91, 0x91, 0x7E}};

int bigWidth(const char *s) {
  int w = 0;
  for (; *s; s++) w += (*s == ':' ? 1 : *s == '-' ? 3 : 4) + 1;
  return w > 0 ? w - 1 : 0;
}

// Цифры, двоеточие и минус шрифтом 4x8; возвращает x сразу за последним столбцом
int drawBig(int x, const char *s, bool colon = true) {
  for (; *s; s++) {
    if (*s == ':') {
      if (colon) { setPixel(x, 2); setPixel(x, 5); }
      x += 2;
    } else if (*s == '-') {
      fillRect(x, 3, x + 2, 3);
      x += 4;
    } else {
      if (*s >= '0' && *s <= '9')
        for (int i = 0; i < 4; i++)
          if (x + i >= 0 && x + i < WIDTH) fb[x + i] |= DIGIT8[*s - '0'][i];
      x += 5;
    }
  }
  return x - 1;
}

// Часы по центру; с погодой — часы, иконка и температура одним размером
void drawClock(bool withWeather) {
  struct tm tm;
  if (!localNow(tm)) {
    drawBig((WIDTH - bigWidth("--:--")) / 2, "--:--");
    return;
  }
  char hm[6], t[6];
  snprintf(hm, sizeof(hm), "%02d:%02d", tm.tm_hour, tm.tm_min);
  bool colon = !cfg.clockBlink || (millis() / 500) % 2 == 0;

  bool w = withWeather && weatherFresh();
  const char *const *icon = nullptr;
  int total = bigWidth(hm);
  if (w) {
    snprintf(t, sizeof(t), "%d", (int)roundf(weather.temp));
    if (strcmp(t, "-0") == 0) strcpy(t, "0");
    icon = weatherIcon(weather.code, weather.day);
    total += 4 + strlen(icon[0]) + 2 + bigWidth(t) + 1 + 3;
  }

  int x = (WIDTH - total) / 2;
  x = drawBig(x, hm, colon);
  if (!w) return;
  x = drawIcon(x + 4, 1, icon, 6) + 2;
  x = drawBig(x, t) + 1;
  setPixel(x + 1, 0); setPixel(x, 1); setPixel(x + 2, 1); setPixel(x + 1, 2);  // градус кружком
}

// Что показать, когда принтер не печатает. false — экран выключить.
bool drawIdle() {
  if (cfg.idleMode == IDLE_TEXT) {
    drawCentered("idle");
    return true;
  }
  if (cfg.idleMode == IDLE_CLOCK || cfg.idleMode == IDLE_CLOCK_WEATHER) {
    drawClock(cfg.idleMode == IDLE_CLOCK_WEATHER);
    return true;
  }
  if (cfg.idleMode == IDLE_RAIN) {
    drawRain();
    return true;
  }
  return false;
}

// Демо из веб-морды: прогресс, нагрев, окончание или пауза без настоящей печати
void drawDemo(bool blink) {
  int pct = min((int)((millis() - demoStart) * 100 / (DEMO_LEN - 1000)), 100);
  if (demoType == "heat") drawHeating(25 + pct * 1.95f, 220, 25 + min(pct * 1.4f, 100.0f) * 0.4f, 65);
  else if (demoType == "done") drawDoneAnim();
  else if (demoType == "pause") drawPause(blink, 42, 83);
  else if (demoType == "rain") drawRain();
  else if (demoType == "cool") {
    float bed = 60 - pct * 0.3f;
    if (bed > cfg.coolTarget) drawCooling(bed, 60, cfg.coolTarget);
    else drawReady();
  }
  else drawProgress(cfg.style, pct, (100 - pct) * 95 / 100, blink, pct);
}

void setDisplayPower(bool on) {
  if (on == !displayOff) return;
  displayOff = !on;
  mx.control(MD_MAX72XX::SHUTDOWN, on ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
  Serial.println(on ? "Display on" : "Display off");
}

// Плавность — «растворение»: у каждой точки свой случайный порог, и при
// fadeLevel от 0 до 1 точки появляются/исчезают в случайном порядке.
// Яркость MAX7219 ступенчатая, так это выглядит плавно на любой яркости.
const uint32_t FADE_MS = 800;
uint16_t fadeRank[WIDTH * 8];
float fadeLevel = 0;
uint8_t lastFrame[WIDTH];  // последний кадр, который тает при выключении

void initFade() {
  for (int i = 0; i < WIDTH * 8; i++) fadeRank[i] = i;
  for (int i = WIDTH * 8 - 1; i > 0; i--) std::swap(fadeRank[i], fadeRank[esp_random() % (i + 1)]);
}

void output(bool on, uint32_t dt) {
  if (on) memcpy(lastFrame, fb, WIDTH);
  if (!cfg.fade) {
    fadeLevel = on ? 1 : 0;
  } else {
    float step = (float)dt / FADE_MS;
    fadeLevel = on ? min(fadeLevel + step, 1.0f) : max(fadeLevel - step, 0.0f);
  }

  if (fadeLevel <= 0) {
    memset(shown, 0, WIDTH);
    setDisplayPower(false);
    return;
  }
  setDisplayPower(true);
  if (fadeLevel >= 1) {
    flush(lastFrame);
    return;
  }
  uint8_t out[WIDTH];
  int threshold = fadeLevel * WIDTH * 8;
  for (int x = 0; x < WIDTH; x++) {
    out[x] = 0;
    for (int y = 0; y < 8; y++)
      if (fadeRank[x * 8 + y] < threshold) out[x] |= lastFrame[x] & (1 << y);
  }
  flush(out);
}

void drawOta();

volatile uint32_t ipShownAt = 0;  // когда подключились к Wi-Fi — один раз прокрутить адрес

// Проверка изображения из веб-морды: тип модулей можно только подобрать на глаз —
// MAX7219 работает на запись, узнать разводку у него нельзя. Показываем тестовую
// картинку в выбранном варианте, ничего не сохраняя; через MODTEST_MS — как было.
volatile int testModule = -1;
volatile uint32_t testUntil = 0;
const uint32_t MODTEST_MS = 30000;
const char *const ICON_ARROW[] = {"0001000", "0001100", "1111110", "1111111", "1111110", "0001100", "0001000"};

bool moduleTestActive() { return testModule >= 0 && (int32_t)(testUntil - millis()) > 0; }

// Подсказка или ошибка подключения к принтеру; false — подсказывать нечего
bool drawPrinterHint() {
  static char text[96];
  static int lastKind = -1;
  static uint32_t since = 0;
  int kind;
  if (!configured()) kind = 0;
  else if (printerError == PERR_CODE) kind = 1;     // неверный код — всегда сообщаем
  else if (printerError == PERR_SERIAL) kind = 2;
  else if (printerError == PERR_NOT_FOUND && !cfg.verified) kind = 3;  // после проверки — это просто «выключен»
  else return false;

  if (kind != lastKind) {
    String ip = WiFi.localIP().toString();
    switch (kind) {
      case 0: snprintf(text, sizeof(text), "SETUP: http://%s", ip.c_str()); break;
      case 1: snprintf(text, sizeof(text), "ACCESS CODE?  http://%s", ip.c_str()); break;
      case 2: snprintf(text, sizeof(text), "SERIAL?  http://%s", ip.c_str()); break;
      default: snprintf(text, sizeof(text), "PRINTER %s NOT FOUND  http://%s", cfg.ip, ip.c_str());
    }
    lastKind = kind;
    since = millis();
  }
  drawScroll(text, since);
  return true;
}

// Заново задаёт настройки MAX7219. При включении питания модули могут поймать помехи
// с линий (GPIO5 дёргается при загрузке ESP32) и уйти в режим теста — тогда горят все точки,
// и ни вывод картинки, ни выключение экрана на это не влияют. Повтор раз в секунду это лечит.
void refreshMatrixRegs(int brightness) {
  mx.control(MD_MAX72XX::TEST, MD_MAX72XX::OFF);
  mx.control(MD_MAX72XX::SCANLIMIT, 7);
  mx.control(MD_MAX72XX::DECODE, MD_MAX72XX::OFF);
  mx.control(MD_MAX72XX::INTENSITY, brightness);
  mx.control(MD_MAX72XX::SHUTDOWN, displayOff ? MD_MAX72XX::ON : MD_MAX72XX::OFF);
}

void render() {
  // Яркость: днём обычная, ночью ночная. Меняет только эта задача — она одна работает с матрицей
  static int appliedBrightness = -1;
  static uint32_t regsAt = 0;
  int want = isNight() ? cfg.nightBrightness : cfg.brightness;
  if (want != appliedBrightness || millis() - regsAt >= 1000) {
    refreshMatrixRegs(want);
    appliedBrightness = want;
    regsAt = millis();
  }
  // Тип модулей: сохранённый или тот, что сейчас проверяют из веб-морды
  static int appliedModule = -1;
  bool modTest = moduleTestActive();
  int module = modTest ? testModule : cfg.moduleType;
  if (module != appliedModule) {
    mx.setModuleType((MD_MAX72XX::moduleType_t)module);
    appliedModule = module;
  }

  fbClear();
  bool blink = (millis() / 500) % 2;
  bool on = true;

  if (otaActive && millis() - otaLast > 10000) otaActive = false;
  if (otaActive) {
    drawOta();
  } else if (modTest) {
    // Читается слева направо и стрелка смотрит вправо — значит, вариант правильный
    drawText(0, "ABC 123");
    drawIcon(56, 0, ICON_ARROW, 7);
  } else if (wm.getConfigPortalActive()) {
    // Подсказка, куда подключаться с телефона
    static uint32_t setupSince = millis();
    drawScroll("SETUP  WIFI: " AP_NAME "  PASS: " AP_PASS, setupSince);
  } else if (demoActive()) {
    drawDemo(blink);
  } else if (WiFi.status() != WL_CONNECTED) {
    drawCentered(blink ? "WiFi..." : "WiFi");
  } else if (ipShownAt && millis() - ipShownAt < 2 * scrollPassMs("IP 255.255.255.255")) {
    // Только подключились к Wi-Fi — показываем адрес веб-морды
    String s = "IP " + WiFi.localIP().toString();
    drawScroll(s.c_str(), ipShownAt);
  } else if (drawPrinterHint()) {
    // нужна настройка или подключение к принтеру не удалось
  } else if (!printerOnline || !haveData) {
    // Принтер выключен или ещё не ответил — экран как в простое
    on = drawIdle();
  } else if (cfg.heatView && heating() &&
             (gcodeState == "PREPARE" || (gcodeState == "RUNNING" && percent <= 0))) {
    // Bambu греется уже в RUNNING с 0%, поэтому смотрим на температуры
    drawHeating(nozzleTemp, nozzleTarget, bedTemp, bedTarget);
  } else if (gcodeState == "RUNNING") {
    drawProgress(cfg.style, percent, remaining, blink, percent);
  } else if (gcodeState == "PAUSE") {
    drawPause(blink, percent, remaining);
  } else if (gcodeState == "PREPARE" || gcodeState == "SLICING") {
    drawPrepare();
  } else if (gcodeState == "FAILED") {
    bool expired = cfg.doneTimeout > 0 && millis() - stateSince > (uint32_t)cfg.doneTimeout * 60000;
    if (expired) on = drawIdle();
    else if (blink) drawCentered("FAIL");
  } else if (gcodeState == "FINISH") {
    uint32_t since = millis() - stateSince;
    // Температура в момент окончания могла ещё не прийти — берём максимум
    if (bedTemp > bedAtFinish) bedAtFinish = bedTemp;
    // Стол остыл (или температура неизвестна) — запоминаем момент
    if (!coolDoneAt && (bedTemp < 0 || bedTemp <= cfg.coolTarget)) coolDoneAt = millis();
    bool cooling = cfg.coolView && !coolDoneAt;
    // С остыванием таймаут считается от READY, иначе — от окончания печати
    uint32_t base = cfg.coolView ? coolDoneAt : stateSince;
    bool expired = cfg.doneTimeout > 0 && base && millis() - base > (uint32_t)cfg.doneTimeout * 60000;
    if (expired) on = drawIdle();
    else if (cfg.doneAnim && since < DONE_ANIM_LEN) drawDoneAnim();
    else if (cooling) drawCooling(bedTemp, bedAtFinish, cfg.coolTarget);
    else if (cfg.coolView) drawReady();
    else drawCentered("DONE");
  } else if (gcodeState == "IDLE") {
    on = drawIdle();
  } else {
    drawCentered(gcodeState.substring(0, 8).c_str());
  }

  static uint32_t lastOutput = 0;
  uint32_t now = millis();
  output(on, lastOutput ? now - lastOutput : 0);
  lastOutput = now;
}

// ---------- MQTT ----------
void onMqttMessage(int size) {
  // Полный отчёт бывает 7+ КБ — парсим прямо из потока,
  // фильтр оставляет только нужные поля, буфер под всё сообщение не нужен.
  JsonDocument filter;
  filter["print"]["gcode_state"] = true;
  filter["print"]["mc_percent"] = true;
  filter["print"]["mc_remaining_time"] = true;
  filter["print"]["layer_num"] = true;
  filter["print"]["total_layer_num"] = true;
  filter["print"]["nozzle_temper"] = true;
  filter["print"]["nozzle_target_temper"] = true;
  filter["print"]["bed_temper"] = true;
  filter["print"]["bed_target_temper"] = true;
  // H2D и другие новые модели: температуры в device.*, текущая и целевая упакованы в одно число
  filter["print"]["device"]["extruder"]["info"][0]["temp"] = true;
  filter["print"]["device"]["bed"]["info"]["temp"] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, mqtt, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("JSON error: %s (size %d)\n", err.c_str(), size);
    return;
  }

  JsonObject p = doc["print"];
  if (p.isNull()) return;
  Lock lock;
  // Принтер может слать только изменившиеся поля, поэтому обновляем то, что пришло
  if (p["gcode_state"].is<const char *>()) {
    String s = p["gcode_state"].as<const char *>();
    if (s != gcodeState) {
      gcodeState = s;
      stateSince = millis();
      bedAtFinish = bedTemp;
      coolDoneAt = 0;
      Serial.printf("State: %s\n", s.c_str());
    }
  }
  if (p["mc_percent"].is<int>()) percent = p["mc_percent"];
  if (p["mc_remaining_time"].is<int>()) remaining = p["mc_remaining_time"];
  if (p["layer_num"].is<int>()) layer = p["layer_num"];
  if (p["total_layer_num"].is<int>()) totalLayers = p["total_layer_num"];
  // Формат новых моделей: temp = (целевая << 16) | текущая. У двух сопел берём то, что греется сильнее
  JsonArray extruders = p["device"]["extruder"]["info"];
  if (!extruders.isNull()) {
    long best = -1;
    for (JsonObject e : extruders) {
      if (!e["temp"].is<long>()) continue;
      long v = e["temp"];
      if ((v >> 16) > (best >> 16) || best < 0) best = v;
    }
    if (best >= 0) {
      nozzleTemp = best & 0xFFFF;
      nozzleTarget = best >> 16;
    }
  }
  if (p["device"]["bed"]["info"]["temp"].is<long>()) {
    long v = p["device"]["bed"]["info"]["temp"];
    bedTemp = v & 0xFFFF;
    bedTarget = v >> 16;
  }
  // Классические поля (X1, P1, A1, P2S) — если есть, они главнее
  if (p["nozzle_temper"].is<float>()) nozzleTemp = p["nozzle_temper"];
  if (p["nozzle_target_temper"].is<float>()) nozzleTarget = p["nozzle_target_temper"];
  if (p["bed_temper"].is<float>()) bedTemp = p["bed_temper"];
  if (p["bed_target_temper"].is<float>()) bedTarget = p["bed_target_temper"];
  haveData = true;
}

// Серийный номер — это CN в сертификате принтера
String serialFromCert() {
  const mbedtls_x509_crt *crt = net.getPeerCertificate();
  if (!crt) return "";
  char dn[128];
  if (mbedtls_x509_dn_gets(dn, sizeof(dn), &crt->subject) < 0) return "";
  String s(dn);
  int i = s.indexOf("CN=");
  if (i < 0) return "";
  s = s.substring(i + 3);
  int comma = s.indexOf(',');
  return comma < 0 ? s : s.substring(0, comma);
}

void requestPushAll() {
  mqtt.beginMessage("device/" + printerSerial + "/request");
  mqtt.print("{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}");
  mqtt.endMessage();
}

// ---------- Поиск принтеров в сети (SSDP) ----------
// Принтеры Bambu сами рассылают объявления о себе (так их находит Bambu Studio):
// NOTIFY на 239.255.255.250:1990 и широковещательно на порт 2021. В них IP, серийник,
// модель и имя. Мы слушаем оба порта и сами время от времени спрашиваем M-SEARCH.
struct FoundPrinter {
  char ip[16];
  char serial[24];
  char model[24];
  char name[40];
  uint32_t seen;
};
FoundPrinter found[6];
int foundCount = 0;
WiFiUDP ssdp1990, ssdp2021;
bool ssdpStarted = false;
uint32_t lastSearch = 0;
uint32_t discoveryWanted = 0;  // веб-морда спрашивала список — ищем активнее

const char *modelName(const char *code) {
  struct { const char *code, *name; } models[] = {
      {"3DPrinter-X1-Carbon", "X1 Carbon"}, {"BL-P001", "X1 Carbon"},
      {"3DPrinter-X1", "X1"}, {"BL-P002", "X1"}, {"C13", "X1E"},
      {"C11", "P1P"}, {"C12", "P1S"}, {"N1", "A1 mini"}, {"N2S", "A1"}, {"O1D", "H2D"},
  };
  for (auto &m : models)
    if (strcmp(code, m.code) == 0) return m.name;
  return code;  // неизвестный код покажем как есть
}

// Значение заголовка "Key: value" из SSDP-пакета (без учёта регистра)
String ssdpHeader(const String &msg, const char *key) {
  String lower = msg;
  lower.toLowerCase();
  String k = String("\n") + key;
  k.toLowerCase();
  int i = lower.indexOf(k + ":");
  if (i < 0) return "";
  i += k.length() + 1;
  int end = msg.indexOf('\r', i);
  if (end < 0) end = msg.indexOf('\n', i);
  String v = msg.substring(i, end < 0 ? msg.length() : end);
  v.trim();
  return v;
}

void rememberPrinter(const String &msg) {
  if (msg.indexOf("bambu") < 0) return;
  // Location — обычно голый IP, но на всякий случай понимаем и URL вида http://ip:port/...
  String ip = ssdpHeader(msg, "Location");
  int scheme = ip.indexOf("://");
  if (scheme >= 0) ip = ip.substring(scheme + 3);
  int tail = ip.indexOf(':');
  if (tail < 0) tail = ip.indexOf('/');
  if (tail >= 0) ip = ip.substring(0, tail);
  String serial = ssdpHeader(msg, "USN");
  int uuid = serial.indexOf("uuid:");  // некоторые прошивки могут прислать USN как uuid:SERIAL::...
  if (uuid >= 0) serial = serial.substring(uuid + 5);
  int colons = serial.indexOf("::");
  if (colons >= 0) serial = serial.substring(0, colons);
  if (ip.isEmpty() || serial.isEmpty()) return;
  Lock lock;
  int slot = -1;
  for (int i = 0; i < foundCount; i++)
    if (serial == found[i].serial) slot = i;
  if (slot < 0) {
    if (foundCount < (int)(sizeof(found) / sizeof(found[0]))) {
      slot = foundCount++;
    } else {
      slot = 0;  // места нет — вытесняем самый давний
      for (int i = 1; i < foundCount; i++)
        if (found[i].seen < found[slot].seen) slot = i;
    }
    Serial.printf("Found printer %s at %s\n", serial.c_str(), ip.c_str());
  }
  FoundPrinter &p = found[slot];
  strlcpy(p.ip, ip.c_str(), sizeof(p.ip));
  strlcpy(p.serial, serial.c_str(), sizeof(p.serial));
  strlcpy(p.model, modelName(ssdpHeader(msg, "DevModel.bambu.com").c_str()), sizeof(p.model));
  strlcpy(p.name, ssdpHeader(msg, "DevName.bambu.com").c_str(), sizeof(p.name));
  p.seen = millis();
}

// Серийник из объявления — запасной вариант, если сертификат не дал CN
String discoveredSerial(const char *ip) {
  for (int i = 0; i < foundCount; i++)
    if (strcmp(found[i].ip, ip) == 0) return found[i].serial;
  return "";
}

void readSsdp(WiFiUDP &udp) {
  int len = udp.parsePacket();
  if (len <= 0) return;
  char buf[768];
  int n = udp.read(buf, min(len, (int)sizeof(buf) - 1));
  if (n <= 0) return;
  buf[n] = 0;
  rememberPrinter(String("\n") + buf);  // \n — чтобы первая строка тоже искалась как заголовок
}

void updateDiscovery() {
  if (!ssdpStarted) {
    ssdp1990.beginMulticast(IPAddress(239, 255, 255, 250), 1990);
    ssdp2021.beginMulticast(IPAddress(239, 255, 255, 250), 2021);
    ssdpStarted = true;
  }
  readSsdp(ssdp1990);
  readSsdp(ssdp2021);

  // Спрашиваем сами: часто, пока принтер не настроен или открыт список, иначе редко
  bool eager = !configured() || !cfg.verified || (discoveryWanted && millis() - discoveryWanted < 60000);
  uint32_t every = eager ? 10000 : 300000;
  if (lastSearch && millis() - lastSearch < every) return;
  lastSearch = millis();
  ssdp1990.beginPacket(IPAddress(239, 255, 255, 250), 1990);
  ssdp1990.print("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1990\r\nMAN: \"ssdp:discover\"\r\n"
                 "MX: 3\r\nST: urn:bambulab-com:device:3dprinter:1\r\n\r\n");
  ssdp1990.endPacket();
}

// Прогресс загрузки приходит на ядре 1, рисует его задача отрисовки
void onOtaProgress(size_t done, size_t total) {
  otaDone = done;
  otaTotal = total;
  otaLast = millis();
  otaActive = true;
}

// Прошивка по Wi-Fi. Два пути, на матрице в обоих OTA и полоса загрузки:
//  - HTTP POST /u (страница «Обновить прошивку», и pio run -e ota через curl);
//  - espota (ArduinoOTA) — нужен входящий порт на компьютере, файрвол macOS его режет.
void setupOta() {
  Update.onProgress(onOtaProgress);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.setMdnsEnabled(false);  // mDNS уже поднят нами
  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
    mqtt.stop();
    otaLast = millis();
    otaActive = true;
  });
  ArduinoOTA.onProgress(onOtaProgress);
  ArduinoOTA.onEnd([]() { Serial.println("OTA done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("OTA error %u\n", e);
    otaActive = false;
  });
  ArduinoOTA.begin();
  MDNS.enableArduino(3232, true);
}

void connectMqtt() {
  Serial.printf("MQTT -> %s:8883\n", cfg.ip);

  mqtt.setUsernamePassword("bblp", cfg.code);
  if (!mqtt.connect(cfg.ip, 8883)) {
    // -1/-2 = нет связи (принтер выключен или не тот IP), 4/5 = неверный Access Code
    int err = mqtt.connectError();
    printerError = (err == 4 || err == 5) ? PERR_CODE : PERR_NOT_FOUND;
    mqttRetry = constrain(mqttRetry * 2, 5000, 30000);
    Serial.printf("MQTT connect failed, err=%d, next try in %us\n", err, (unsigned)(mqttRetry / 1000));
    return;
  }
  mqttRetry = 5000;
  printerSerial = cfg.serial[0] ? String(cfg.serial) : serialFromCert();
  if (printerSerial.isEmpty()) printerSerial = discoveredSerial(cfg.ip);
  if (printerSerial.isEmpty()) {
    Serial.println("Can't read printer serial from certificate, set it in the web UI");
    printerError = PERR_SERIAL;
    mqtt.stop();
    return;
  }
  mqtt.subscribe("device/" + printerSerial + "/report");
  requestPushAll();
  printerOnline = true;
  printerError = PERR_NONE;
  if (!cfg.verified) {
    Lock lock;
    cfg.verified = true;
    saveConfig();
  }
  Serial.printf("MQTT connected, printer %s\n", printerSerial.c_str());
}

// Код погоды met.no ("partlycloudy_day", "lightrainshowers_night"…) → код WMO, как у Open-Meteo
int wmoFromMetNo(const char *s) {
  if (!s) return -1;
  if (strstr(s, "thunder")) return 95;
  if (strstr(s, "snow")) return 71;
  if (strcmp(s, "lightrain") == 0) return 51;  // морось
  if (strstr(s, "rain") || strstr(s, "sleet")) return 61;
  if (strstr(s, "fog")) return 45;
  if (strncmp(s, "clearsky", 8) == 0) return 0;
  if (strncmp(s, "fair", 4) == 0) return 1;
  if (strncmp(s, "partlycloudy", 12) == 0) return 2;
  return 3;  // cloudy
}

// Сдвиг местного времени от UTC в минутах — тот же, что у часов
long localOffsetMin() {
  if (cfg.tzOffset != TZ_AUTO) return cfg.tzOffset;
  return weather.utcOffset / 60;
}

// Обёртка над потоком ответа: ArduinoJson читает через readBytes(), а у сетевого
// клиента он свой и не ждёт данных — 40 КБ от met.no по слабому Wi-Fi обрывались
// на полуслове (IncompleteInput). Здесь readBytes() — обычный, с ожиданием до таймаута.
struct WaitingStream : Stream {
  Stream &s;
  WaitingStream(Stream &s) : s(s) { setTimeout(10000); }
  int available() override { return s.available(); }
  int read() override { return s.read(); }
  int peek() override { return s.peek(); }
  size_t write(uint8_t) override { return 0; }
};

// Один GET по HTTPS; при успехе разбирает ответ в doc (с фильтром, потоком)
bool weatherGet(const char *url, JsonDocument &doc, const JsonDocument &filter) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  http.useHTTP10(true);  // без chunked — можно разбирать прямо из потока
  // met.no требует User-Agent с контактом, без него отвечает 403
  http.setUserAgent("bambu-matrix/" FW_VERSION " github.com/" UPDATE_REPO);
  String host = String(url).substring(8, String(url).indexOf('/', 8));
  if (!http.begin(client, url)) return false;
  int status = http.GET();
  if (status != 200) {
    char tls[64] = "";
    if (status < 0) client.lastError(tls, sizeof(tls));
    String e = host + ": " + (status < 0 ? HTTPClient::errorToString(status) + " " + tls : "HTTP " + String(status));
    Serial.println("Weather " + e);
    weather.err += (weather.err.length() ? "; " : "") + e;
    http.end();
    return false;
  }
  WaitingStream body(http.getStream());
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    String e = host + ": bad response (" + err.c_str() + ")";
    Serial.println("Weather " + e);
    weather.err += (weather.err.length() ? "; " : "") + e;
  }
  return !err;
}

// Open-Meteo: погода, восход/закат и часовой пояс одним запросом
bool fetchOpenMeteo() {
  char url[220];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day&daily=sunrise,sunset&forecast_days=1&timezone=auto",
           cfg.lat, cfg.lon);
  JsonDocument filter, doc;
  filter["current"] = true;
  filter["daily"] = true;
  filter["utc_offset_seconds"] = true;
  if (!weatherGet(url, doc, filter) || !doc["current"]["temperature_2m"].is<float>()) return false;
  Lock lock;
  weather.temp = doc["current"]["temperature_2m"];
  weather.code = doc["current"]["weather_code"] | 0;
  weather.day = (doc["current"]["is_day"] | 1) == 1;
  weather.utcOffset = doc["utc_offset_seconds"] | 0L;
  weather.sunrise = minutesFromIso(doc["daily"]["sunrise"][0]);
  weather.sunset = minutesFromIso(doc["daily"]["sunset"][0]);
  return true;
}

// Запасной вариант — MET Norway (api.met.no): когда Open-Meteo недоступен
// (например, его сервер блокирует провайдер). Часовой пояс он не знает: берём
// ручной, а в режиме «Авто» — тот, что веб-морда посчитала при выборе города.
bool fetchMetNo() {
  char url[200];
  snprintf(url, sizeof(url), "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=%.4f&lon=%.4f",
           cfg.lat, cfg.lon);
  JsonDocument filter, doc;
  JsonObject f = filter["properties"]["timeseries"][0]["data"].to<JsonObject>();
  f["instant"]["details"]["air_temperature"] = true;
  f["next_1_hours"]["summary"]["symbol_code"] = true;
  if (!weatherGet(url, doc, filter)) return false;
  JsonObject now = doc["properties"]["timeseries"][0]["data"];
  if (!now["instant"]["details"]["air_temperature"].is<float>()) return false;
  const char *sym = now["next_1_hours"]["summary"]["symbol_code"];
  {
    Lock lock;
    weather.temp = now["instant"]["details"]["air_temperature"];
    weather.code = wmoFromMetNo(sym);
    if (weather.code < 0) weather.code = 3;
    weather.day = !sym || !strstr(sym, "_night");
    if (cfg.tzOffset == TZ_AUTO && cfg.cityTz != TZ_AUTO) weather.utcOffset = cfg.cityTz * 60L;
  }

  // Восход и закат — отдельным запросом, в UTC; на местное время переводим сами
  snprintf(url, sizeof(url), "https://api.met.no/weatherapi/sunrise/3.0/sun?lat=%.4f&lon=%.4f&offset=%%2B00:00",
           cfg.lat, cfg.lon);
  JsonDocument sFilter, sun;
  sFilter["properties"]["sunrise"]["time"] = true;
  sFilter["properties"]["sunset"]["time"] = true;
  if (weatherGet(url, sun, sFilter)) {
    long off = localOffsetMin();
    int rise = minutesFromIso(sun["properties"]["sunrise"]["time"]);
    int set = minutesFromIso(sun["properties"]["sunset"]["time"]);
    Lock lock;
    weather.sunrise = rise < 0 ? -1 : (int)(((rise + off) % 1440 + 1440) % 1440);
    weather.sunset = set < 0 ? -1 : (int)(((set + off) % 1440 + 1440) % 1440);
  }
  return true;
}

// Раз в 15 минут (при ошибке — через минуту). Крутится в loop() на ядре 1,
// пока идёт запрос, матрица на ядре 0 продолжает рисовать. Сначала пробуем
// источник, который сработал в прошлый раз, чтобы не ждать таймаута каждый раз.
void updateWeather() {
  if (!cfg.city[0] || otaActive) return;
  uint32_t wait = weather.updated ? WEATHER_EVERY : WEATHER_RETRY;
  if (lastWeatherTry && millis() - lastWeatherTry < wait) return;
  lastWeatherTry = millis();

  long oldOffset = weather.utcOffset;
  weather.err = "";
  bool metFirst = weather.source == 2;
  int got = 0;
  if (metFirst ? fetchMetNo() : fetchOpenMeteo()) got = metFirst ? 2 : 1;
  else if (metFirst ? fetchOpenMeteo() : fetchMetNo()) got = metFirst ? 1 : 2;

  Lock lock;
  weather.failed = !got;
  if (!got) {
    weather.updated = 0;
    return;
  }
  weather.source = got;
  weather.updated = millis();
  if (cfg.tzOffset == TZ_AUTO && weather.utcOffset != oldOffset) applyTimezone();
  Serial.printf("Weather (%s): %.1f C, code %d\n", got == 1 ? "open-meteo" : "met.no", weather.temp, weather.code);
}

// ---------- Обновления с GitHub ----------
// Только по кнопке в веб-морде: сама матрица на GitHub не ходит.
// Последний релиз берём из API GitHub, файл прошивки — из его вложений. Соединения
// проверяются по корневым сертификатам из github_ca.h: прошивка ставится только
// если она действительно пришла с GitHub. Работает для публичного репозитория.
struct UpdateInfo {
  String latest;          // тег последнего релиза
  String notes;           // описание релиза (обрезано)
  String url;             // ссылка на bambu-matrix-ota.bin
  bool available = false; // релиз новее текущей версии
  String error;           // "", "network", "no_release", "no_asset", "http", "install"
  uint32_t checkedAt = 0; // millis() последней проверки, 0 — ещё не проверяли
} upd;
volatile bool updCheckRequested = false, updInstallRequested = false;
volatile int updState = 0;  // 0 — ничего, 1 — проверяем, 2 — ставим

// "v1.2.3", "1.2.3-4-gabc" -> {1,2,3}. Хэш или "dev" не разбираются
bool parseVersion(const String &v, int out[3]) {
  const char *s = v.c_str();
  if (*s == 'v' || *s == 'V') s++;
  return sscanf(s, "%d.%d.%d", &out[0], &out[1], &out[2]) == 3;
}

// Сборка без тега (хэш, dev) считается старше любого релиза
bool isNewer(const String &latest, const String &current) {
  int a[3], b[3];
  if (!parseVersion(latest, a)) return false;
  if (!parseVersion(current, b)) return true;
  for (int i = 0; i < 3; i++)
    if (a[i] != b[i]) return a[i] > b[i];
  return false;
}

void checkForUpdate() {
  updState = 1;
  WiFiClientSecure client;
  client.setCACert(GITHUB_CA);
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("bambu-matrix/" FW_VERSION);  // без User-Agent GitHub отвечает 403
  String error, latest, notes, url;
  if (!http.begin(client, "https://api.github.com/repos/" UPDATE_REPO "/releases/latest")) {
    error = "network";
  } else {
    http.addHeader("Accept", "application/vnd.github+json");
    int status = http.GET();
    if (status == 200) {
      String body = http.getString();  // getString сам разбирает chunked-ответ
      JsonDocument filter;
      filter["tag_name"] = true;
      filter["body"] = true;
      filter["assets"][0]["name"] = true;
      filter["assets"][0]["browser_download_url"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
        error = "http";
      } else {
        latest = doc["tag_name"] | "";
        notes = doc["body"] | "";
        if (notes.length() > 600) notes = notes.substring(0, 600) + "…";
        for (JsonObject a : doc["assets"].as<JsonArray>())
          if (a["name"] == UPDATE_ASSET) url = a["browser_download_url"] | "";
        if (url.isEmpty()) error = "no_asset";
      }
    } else {
      // 404 — релизов нет или репозиторий приватный; <0 — нет связи или не прошла проверка сертификата
      error = status == 404 ? "no_release" : status < 0 ? "network" : "http";
      Serial.printf("Update check: HTTP %d\n", status);
    }
    http.end();
  }

  Lock lock;
  upd.latest = latest;
  upd.notes = notes;
  upd.url = url;
  upd.error = error;
  upd.available = error.isEmpty() && isNewer(latest, FW_VERSION);
  upd.checkedAt = millis() | 1;
  updState = 0;
  Serial.printf("Update check: current %s, latest %s%s\n", FW_VERSION, latest.c_str(),
                upd.available ? " (available)" : "");
}

// Скачивает прошивку с GitHub и пишет во второй раздел приложения; настройки не трогаются.
// Пока идёт загрузка, задача отрисовки показывает на матрице OTA и полосу.
void installUpdate() {
  String url;
  {
    Lock lock;
    if (!upd.available || upd.url.isEmpty()) return;
    url = upd.url;
  }
  updState = 2;
  Serial.printf("Installing update from %s\n", url.c_str());
  mqtt.stop();
  otaLast = millis();
  otaActive = true;

  WiFiClientSecure client;
  client.setCACert(GITHUB_CA);
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent("bambu-matrix/" FW_VERSION);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // github.com -> release-assets.githubusercontent.com
  bool ok = false;
  if (http.begin(client, url) && http.GET() == 200) {
    int len = http.getSize();
    if (len > 0 && Update.begin(len)) {
      size_t written = Update.writeStream(*http.getStreamPtr());
      ok = written == (size_t)len && Update.end(true);
      if (!ok) Update.abort();
    }
  }
  http.end();
  if (ok) {
    Serial.println("Update installed, rebooting");
    delay(500);
    ESP.restart();
  }
  Serial.println("Update failed");
  Lock lock;
  upd.error = "install";
  otaActive = false;
  updState = 0;
}

void maintainUpdates() {
  if (WiFi.status() != WL_CONNECTED || wm.getConfigPortalActive()) return;
  if (updInstallRequested) {
    updInstallRequested = false;
    installUpdate();
    return;
  }
  if (updCheckRequested) {
    updCheckRequested = false;
    checkForUpdate();
  }
}

// ---------- Веб-морда ----------
void handleRoot() {
  // В режиме точки доступа корень нужен WiFiManager'у
  if (wm.getConfigPortalActive()) {
    wm.server->sendHeader("Location", "/wifi");
    wm.server->send(302);
    return;
  }
  wm.server->send_P(200, "text/html; charset=utf-8", WEB_PAGE);
}

void handleState() {
  Lock lock;
  JsonDocument doc;
  doc["wifi"] = WiFi.RSSI();
  doc["mqtt"] = mqtt.connected();
  doc["serial"] = printerSerial;
  doc["state"] = gcodeState;
  doc["percent"] = percent;
  doc["remaining"] = remaining;
  doc["layer"] = layer;
  doc["layers"] = totalLayers;
  doc["nozzle"] = nozzleTemp;
  doc["nozzleTarget"] = nozzleTarget;
  doc["bed"] = bedTemp;
  doc["bedTarget"] = bedTarget;
  doc["display"] = fadeLevel > 0;
  doc["demo"] = demoActive();
  doc["uptime"] = millis() / 1000;
  doc["night"] = isNight();
  doc["configured"] = configured();
  doc["verified"] = cfg.verified;
  static const char *const errors[] = {"", "not_found", "code", "serial"};
  doc["error"] = errors[printerError];
  doc["ip"] = WiFi.localIP().toString();
  doc["fw"] = FW_VERSION;
  JsonObject u = doc["upd"].to<JsonObject>();
  u["state"] = updState;
  u["checked"] = upd.checkedAt != 0;
  u["latest"] = upd.latest;
  u["available"] = upd.available;
  u["notes"] = upd.notes;
  u["error"] = upd.error;
  if (weather.sunrise >= 0) {
    doc["sunrise"] = weather.sunrise;
    doc["sunset"] = weather.sunset;
  }
  if (weather.failed) doc["wFail"] = true;
  if (weather.err.length()) doc["wErr"] = weather.err;
  if (weatherFresh()) {
    doc["wTemp"] = weather.temp;
    doc["wCode"] = weather.code;
    doc["wDay"] = weather.day;
    doc["wAge"] = (millis() - weather.updated) / 60000;
    doc["wSrc"] = weather.source == 2 ? "met.no" : "open-meteo.com";
  }
  char hex[WIDTH * 2 + 1];
  for (int i = 0; i < WIDTH; i++) sprintf(hex + i * 2, "%02x", shown[i]);
  doc["fb"] = hex;
  String out;
  serializeJson(doc, out);
  wm.server->send(200, "application/json", out);
}

void handleGetSettings() {
  JsonDocument doc;
  configToJson(doc);
  String out;
  serializeJson(doc, out);
  wm.server->send(200, "application/json", out);
}

void handleSaveSettings() {
  JsonDocument doc;
  if (deserializeJson(doc, wm.server->arg("plain"))) {
    wm.server->send(400, "text/plain", "bad json");
    return;
  }
  Lock lock;
  String oldIp = cfg.ip, oldCode = cfg.code, oldSerial = cfg.serial;
  int oldTz = cfg.tzOffset;
  float oldLat = cfg.lat, oldLon = cfg.lon;
  doc.remove("verified");  // это состояние, а не настройка — из браузера не принимаем
  configFromJson(doc);
  saveConfig();

  if (cfg.tzOffset != oldTz) applyTimezone();
  if (oldIp != cfg.ip || oldCode != cfg.code || oldSerial != cfg.serial) {
    reconnectPrinter();
    saveConfig();  // сбросили verified
  }
  if (oldLat != cfg.lat || oldLon != cfg.lon) {
    weather.updated = 0;  // новый город — обновить сразу
    lastWeatherTry = 0;
  }
  pIp.setValue(cfg.ip, sizeof(cfg.ip) - 1);
  pCode.setValue(cfg.code, sizeof(cfg.code) - 1);
  wm.server->send(200, "application/json", "{\"ok\":true}");
}

// «Забыть принтер»: IP, код и серийник стираются, матрица снова ждёт настройки (SETUP)
void handleForgetPrinter() {
  Lock lock;
  cfg.ip[0] = cfg.code[0] = cfg.serial[0] = '\0';
  reconnectPrinter();
  printerSerial = "";
  gcodeState = "";
  saveConfig();
  pIp.setValue("", sizeof(cfg.ip) - 1);
  pCode.setValue("", sizeof(cfg.code) - 1);
  Serial.println("Printer forgotten");
  wm.server->send(200, "application/json", "{\"ok\":true}");
}

void handlePrinters() {
  Lock lock;
  discoveryWanted = millis();
  JsonDocument doc;
  JsonArray list = doc["printers"].to<JsonArray>();
  for (int i = 0; i < foundCount; i++) {
    if (millis() - found[i].seen > 15 * 60000) continue;  // давно не объявлялся
    JsonObject p = list.add<JsonObject>();
    p["ip"] = found[i].ip;
    p["serial"] = found[i].serial;
    p["model"] = found[i].model;
    p["name"] = found[i].name;
  }
  String out;
  serializeJson(doc, out);
  wm.server->send(200, "application/json", out);
}

// POST /api/modtest?type=N — показать тестовую картинку с типом модулей N (0..3), -1 — закончить
void handleModTest() {
  Lock lock;
  int type = wm.server->hasArg("type") ? wm.server->arg("type").toInt() : -1;
  testModule = (type >= 0 && type <= 3) ? type : -1;
  testUntil = millis() + MODTEST_MS;
  wm.server->send(200, "application/json", "{\"ok\":true}");
}

void handleDemo() {
  Lock lock;
  demoType = wm.server->hasArg("type") ? wm.server->arg("type") : "progress";
  demoStart = millis();
  wm.server->send(200, "application/json", "{\"ok\":true}");
}

// WiFiManager вызывает это до регистрации своих страниц, поэтому «/» достаётся нам
void bindWebRoutes() {
  wm.server->on("/", HTTP_GET, handleRoot);
  wm.server->on("/api/state", HTTP_GET, handleState);
  wm.server->on("/api/settings", HTTP_GET, handleGetSettings);
  wm.server->on("/api/settings", HTTP_POST, handleSaveSettings);
  wm.server->on("/api/demo", HTTP_POST, handleDemo);
  wm.server->on("/api/printers", HTTP_GET, handlePrinters);
  wm.server->on("/api/printer/forget", HTTP_POST, handleForgetPrinter);
  wm.server->on("/api/modtest", HTTP_POST, handleModTest);
  wm.server->on("/api/update/check", HTTP_POST, []() {
    updCheckRequested = true;
    wm.server->send(200, "application/json", "{\"ok\":true}");
  });
  wm.server->on("/api/update/install", HTTP_POST, []() {
    updInstallRequested = upd.available;
    wm.server->send(upd.available ? 200 : 409, "application/json",
                    upd.available ? "{\"ok\":true}" : "{\"ok\":false}");
  });
}

// GPIO0 на GND 3 с — открыть точку доступа для настройки
void checkButton() {
  static uint32_t pressedAt = 0;
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!pressedAt) pressedAt = millis();
    if (millis() - pressedAt > 3000 && !wm.getConfigPortalActive()) {
      Serial.println("Button: starting config portal");
      mqtt.stop();
      if (wm.getWebPortalActive()) wm.stopWebPortal();
      wm.startConfigPortal(AP_NAME, AP_PASS);
      manualPortal = true;
    }
  } else {
    pressedAt = 0;
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    // 201 = сеть не найдена (часто 5 ГГц), 15/204 = неверный пароль
    Serial.printf("WiFi disconnected, reason=%d\n", info.wifi_sta_disconnected.reason);
  else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("WiFi OK, IP=%s, http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
    ipShownAt = millis();
  }
}

// Пропал Wi-Fi — переподключаемся сами, SETUP только если сети нет дольше WIFI_GIVE_UP.
// Пока открыт SETUP, тоже пробуем сеть (если к точке доступа никто не подключён)
// и закрываем портал, как только подключились.
void maintainWifi() {
  static bool wasPortal = false;
  bool portal = wm.getConfigPortalActive();
  if (!portal) manualPortal = false;
  // Портал закрылся по таймауту — снова WIFI_GIVE_UP пробуем сеть, потом SETUP
  if (wasPortal && !portal && wifiLostSince) wifiLostSince = millis();
  wasPortal = portal;

  if (WiFi.status() == WL_CONNECTED) {
    wifiLostSince = 0;
    if (portal && !manualPortal) {
      Serial.println("WiFi back, closing config portal");
      wm.stopConfigPortal();
      WiFi.mode(WIFI_STA);
    }
    return;
  }
  if (wm.getWiFiSSID(true).isEmpty()) return;  // сеть ещё не настроена — ждём в SETUP
  if (!wifiLostSince) wifiLostSince = millis();

  if (!portal && millis() - wifiLostSince > WIFI_GIVE_UP) {
    Serial.println("No WiFi for too long, starting config portal");
    mqtt.stop();
    if (wm.getWebPortalActive()) wm.stopWebPortal();
    wm.startConfigPortal(AP_NAME, AP_PASS);
    lastWifiTry = millis();
    return;
  }

  if (millis() - lastWifiTry > WIFI_RETRY && (!portal || WiFi.softAPgetStationNum() == 0)) {
    lastWifiTry = millis();
    Serial.println("WiFi retry");
    WiFi.begin();
  }
}

// ---------- Задача отрисовки (ядро 0) ----------
void drawOta() {
  drawText(0, "OTA");
  int k = otaTotal ? (uint64_t)otaDone * 42 / otaTotal : 0;
  drawFrame(21, 63);
  if (k) fillRect(22, 2, 21 + k, 5);
}

void renderTask(void *) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    {
      Lock lock;
      render();
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(50));
  }
}

// ---------- Main ----------
void setup() {
  Serial.begin(115200);
  Serial.println("\nBambu matrix " FW_VERSION);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  LittleFS.begin(true);  // true — отформатировать при первом запуске
  loadConfig();

  stateLock = xSemaphoreCreateMutex();
  delay(200);  // даём питанию матрицы установиться, иначе модули могут пропустить начальную настройку
  mx.begin();
  initFade();
  mx.control(MD_MAX72XX::INTENSITY, cfg.brightness);
  xTaskCreatePinnedToCore(renderTask, "render", 8192, nullptr, 1, nullptr, 0);

  pIp.setValue(cfg.ip, sizeof(cfg.ip) - 1);
  pCode.setValue(cfg.code, sizeof(cfg.code) - 1);

  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  wm.addParameter(&pHint);
  wm.addParameter(&pIp);
  wm.addParameter(&pCode);
  wm.setTitle("Bambu Matrix");
  wm.setHostname(HOSTNAME);
  static std::vector<const char *> menu = {"wifi", "param", "info", "update", "restart"};
  wm.setMenu(menu);
  wm.setSaveParamsCallback(onParamsSaved);
  wm.setWebServerCallback(bindWebRoutes);
  wm.setConfigPortalBlocking(false);  // чтобы матрица жила, пока открыт портал
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(120);     // через 2 минуты без действий портал закрывается

  // Ищем роутер по всем каналам и берём самый сильный сигнал (важно для mesh)
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.setAutoReconnect(true);

  String ssid = wm.getWiFiSSID(true);
  Serial.printf("Saved WiFi: '%s'\n", ssid.c_str());
  if (ssid.isEmpty()) {
    wm.startConfigPortal(AP_NAME, AP_PASS);
  } else {
    WiFi.begin();
    wifiLostSince = lastWifiTry = millis();
  }
  applyTimezone();

  net.setInsecure();  // у принтера самоподписанный сертификат
  mqtt.setId(String("bambu-matrix-") + String((uint32_t)ESP.getEfuseMac(), HEX));
  mqtt.setKeepAliveInterval(30 * 1000);
  mqtt.setConnectionTimeout(10 * 1000);
  mqtt.onMessage(onMqttMessage);
}

void loop() {
  wm.process();
  checkButton();
  maintainWifi();

  if (WiFi.status() == WL_CONNECTED && !wm.getConfigPortalActive()) {
    // Веб-морда остаётся доступной в домашней сети
    if (!wm.getWebPortalActive()) wm.startWebPortal();
    if (!mdnsStarted) {
      mdnsStarted = MDNS.begin(HOSTNAME);
      MDNS.addService("http", "tcp", 80);
      setupOta();
    }
    ArduinoOTA.handle();
    updateDiscovery();

    if (configured()) {
      if (mqtt.connected()) {
        mqtt.poll();
      } else if (millis() - lastMqttTry > mqttRetry) {
        printerOnline = false;
        haveData = false;
        lastMqttTry = millis();
        connectMqtt();  // может занять несколько секунд — матрица при этом не замирает
      }
    }
  }
  if (WiFi.status() == WL_CONNECTED && !wm.getConfigPortalActive()) updateWeather();
  maintainUpdates();
  printerOnline = WiFi.status() == WL_CONNECTED && mqtt.connected();
}
