#include <NTPClient.h>
#include "WiFiS3.h"
#include "ArduinoGraphics.h"

#include "Arduino_LED_Matrix.h"
#include <WiFiUdp.h>

#include "http_server.h"
#include "svgs.h"
#include "style_css.h"
#include "network_info.h"
#include "offsets.h"
#include "animation.h"

#define MAX_REQUEST_SIZE 2048
#define RESPONSE_BUFF_SIZE 1024
#define MAX_MEETING_COUNT 30
#define MAX_OOO_COUNT 3

char ssid[] = NETWORK_SSID;
char pass[] = NETWORK_PASS;
IPAddress local_ip = LOCAL_IP;
IPAddress dns = DNS;
IPAddress gateway = GATEWAY;
IPAddress subnet = SUBNET;

const char camera_svg[] PROGMEM = CAMERA_SVG;
const char green_check_svg[] PROGMEM = GREEN_CHECK_SVG;
const char yellow_dash_svg[] PROGMEM = YELLOW_DASH_SVG;
const char red_dash_svg[] PROGMEM = RED_DASH_SVG;
const char grey_circle_svg[] PROGMEM = GREY_CIRCLE_SVG;
const char css[] PROGMEM = CSS;

ArduinoLEDMatrix matrix;

#define SATURDAY 6
#define SUNDAY 0

WiFiUDP ntp_udp;
NTPClient time_client(ntp_udp);

WiFiServer server(80);

class Time {
  public:
    Time() : _t(0), _offset(0) {}
    Time(unsigned long t) : _t(t), _offset(getOffset(t)) {}

    static Time now() {
      return Time(time_client.getEpochTime());   
    }

    int getHour() {
      return (getTimeWithOffset() % 86400L) / 3600;
    }

    int getMinute() {
      return (getTimeWithOffset() % 3600) / 60;
    }

    int getSecond() {
      return getTimeWithOffset() % 60;
    }

    int getDay() {
      return (getDate() - 4) % 7;
    }

    int getDate() {
      return getTimeWithOffset()  / 86400L;
    }

    unsigned long getTimeWithOffset() {
      return _t + _offset;
    }

    unsigned long getTime() {
      return _t;
    }

    void prettyPrint(BufferedResponseWriter& writer) {
      int hour = getHour();
      int minute = getMinute();
      int second = getSecond();

      const char *amPm = hour > 11 ? "PM" : "AM";
      int modHour = hour % 12;
      if (modHour == 0) {
        modHour = 12;
      }
      appendLong(writer, modHour, false);
      writer.write(":");
      appendLong(writer, minute, true);
      if (second != 0) {
        writer.write(":");
        appendLong(writer, second, true);
      }
      writer.write(" ");
      writer.write(amPm);
    }

    bool isBefore(const Time& rhs) {
      return _t < rhs._t;
    }

    bool isAfter(const Time& rhs) {
      return _t > rhs._t;
    }

  private:

    void appendLong(BufferedResponseWriter& writer, int n, bool pad) {
      if (n < 10 && pad) {
        writer.write("0");
      }
      char digits[3];
      ltoa(n, digits, 10);
      writer.write(digits);
    }

    unsigned long _t;
    int _offset;
};

class TimeRange {
  public:
    TimeRange() : _start(Time()), _end(Time()) {}
    TimeRange(Time start, Time end) : _start(start), _end(end) {}
    bool isEmpty() { return _start.getTime() == 0; }
    Time* getStart() { return &_start; }
    Time* getEnd() { return &_end; }

    void prettyPrint(BufferedResponseWriter& writer) {
      _start.prettyPrint(writer);
      writer.write(" - ");
      _end.prettyPrint(writer);
    }

    bool isInThePast(Time& now) {
      return _end.isBefore(now);
    }

    bool isInTheFuture(Time& now) {
      return _start.isAfter(now);
    }

    bool isCurrentlyHappening(Time& now) {
      return _start.isBefore(now) && _end.isAfter(now);
    }

  private:
    Time _start;
    Time _end;
};

struct LedColor {
  const int pin;
  const int brightness;
  const bool off;
  const char *svg;
  const char *message;
};

LedColor ALL_OFF = {
  /*pin=*/ -1,
  /*brightness=*/ -1,
  /*off=*/true,
  /*svg=*/"grey_circle.svg",
  /*message=*/"Not working",
};
LedColor RED = {
  /*pin=*/10,
  /*brightness=*/20,
  /*off=*/false,
  /*svg=*/"red_dash.svg",
  /*message=*/"In a meeting",
};
LedColor YELLOW = {
  /*pin=*/11,
  /*brightness=*/255,
  /*off=*/false,
  /*svg=*/"yellow_dash.svg",
  /*message=*/"Meeting starting soon",
};
LedColor GREEN = {
  /*pin=*/9,
  /*brightness=*/60,
  /*off=*/false,
  /*svg=*/"green_check.svg",
  /*message=*/"No meeting!",
};
boolean in_meeting = false;
LedColor *current_led_color = &ALL_OFF;
LedColor *led_color_from_schedule = &ALL_OFF;

const uint32_t exclamation[] = {
  0x6006006,
  0x600600,
  0x60060,
};

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  matrix.begin();
  matrix.loadSequence(startup_animation);
  matrix.play(true);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    matrix.loadFrame(exclamation);
    while (true);
  }
  WiFi.config(local_ip, dns, gateway, subnet);
  WiFi.begin(ssid, pass);

  int status_attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (status_attempt++ > 6) {
      matrix.loadFrame(exclamation);
    }
  }

  server.begin();
  IPAddress ip = WiFi.localIP();
  Serial.print("Listening at: http://");
  Serial.println(ip);

  time_client.begin();

  initLedPin(RED);
  initLedPin(GREEN);
  initLedPin(YELLOW);

  setLedColor(&RED);
  delay(250);
  setLedColor(&GREEN);
  delay(250);
  setLedColor(&YELLOW);
  delay(250);
  setLedColor(&ALL_OFF);

  matrix.loadFrame(camera);
}

void initLedPin(LedColor led_color) {
  pinMode(led_color.pin, OUTPUT);
  analogWrite(led_color.pin, 0);
}

char request_buff[MAX_REQUEST_SIZE];
char response_buff[MAX_REQUEST_SIZE];

TimeRange meetings[MAX_MEETING_COUNT];
TimeRange ooos[MAX_OOO_COUNT];
TimeRange current_meeting = TimeRange();
TimeRange next_meeting = TimeRange();
bool currently_ooo;

void loop() {
  time_client.update();
  updateCurrentMeeting();
  updateLedBasedOnSchedule();
  maybeHandleHttpRequest();
}

void maybeHandleHttpRequest() {
  WiFiClient client = server.available();
  if (client) {
    BufferedResponseWriter writer(&client, response_buff, sizeof(response_buff));
    boolean responseSent = false;
    int index = 0;
    int deadline = millis() + 2000;
    while (client.connected() && millis() < deadline && !responseSent) {
      int available = client.available();
      if (available > 0) {
        int read_length = max(available, sizeof(request_buff) - 1 - index);
        int read = client.read((uint8_t*) request_buff + index, read_length);
        index += read;
        int search_start = max(0, index - read - 2);
        if (strstr(request_buff + search_start, "\n\r\n") != NULL) {
          StringView requestView = StringView(request_buff, index);
          StringView firstLine = requestView.substring(0, requestView.indexOf("\n"));
          int firstSpace = firstLine.indexOf(" ");
          if (firstSpace > 0) {
            StringView method = firstLine.substring(0, firstSpace);
            StringView remaining = firstLine.substring(firstSpace + 1, firstLine.length() - 1);
            int secondSpace = remaining.indexOf(" ");
            if (secondSpace > 0) {
              StringView resource = remaining.substring(0, secondSpace);
              dispatchRequest(client, writer, method, resource);
              responseSent = true;
            }
          }
          if (!responseSent) {
            sendResponseHeaders(writer, 400, "", "");
          }
        }
      }
    }
    if (millis() >= deadline) {
      Serial.println("deadline exceeded");
    }
    writer.flush();
    // give the web browser time to receive the data
    delay(1);

    client.stop();
    memset(request_buff, 0, sizeof(request_buff));
  }
}

void dispatchRequest(WiFiClient client, BufferedResponseWriter& writer, StringView method, StringView resource) {
  Serial.write("Dispatching ");
  resource.writeToSerial();
  Serial.println();
  StringView path;
  StringView query;
  int questionMarkIndex = resource.indexOf("?");
  if (questionMarkIndex > 0) {
    path = resource.substring(0, questionMarkIndex);
    query = resource.substring(questionMarkIndex + 1);
  } else {
    path = resource;
    query = StringView("", 0);
  }

  if (path.equals("/favicon.ico") || path.equals("/favicon.svg")) {
    sendFile(client, writer, camera_svg, sizeof(CAMERA_SVG) - 1, "image/svg+xml");
  } else if (path.equals("/style.css")) {
    sendFile(client, writer, css, sizeof(CSS) - 1, "text/css");
  } else if (path.equals("/green_check.svg")) {
    sendFile(client, writer, green_check_svg, sizeof(GREEN_CHECK_SVG) - 1, "image/svg+xml");
  } else if (path.equals("/yellow_dash.svg")) {
    sendFile(client, writer, yellow_dash_svg, sizeof(YELLOW_DASH_SVG) - 1, "image/svg+xml");
  } else if (path.equals("/red_dash.svg")) {
    sendFile(client, writer, red_dash_svg, sizeof(RED_DASH_SVG) - 1, "image/svg+xml");
  } else if (path.equals("/grey_circle.svg")) {
    sendFile(client, writer, grey_circle_svg, sizeof(GREY_CIRCLE_SVG) - 1, "image/svg+xml");
  } else if (path.equals("/meetingStarted")) {
    meetingStarted(writer);
  } else if (path.equals("/meetingEnded")) {
    meetingEnded(writer);
  } else if (path.equals("/setSchedule")) {
    setSchedule(query, writer);
  } else {
    sendStatus(writer);
  }
}

void sendFile(WiFiClient client, BufferedResponseWriter& writer, const char *file_contents, int len, const char *content_type) {
  sendResponseHeaders(writer, 200, "Cache-Control: public, max-age=691200", content_type);
  writer.write_P(file_contents, len);
}

void meetingStarted(BufferedResponseWriter& writer) {
  in_meeting = true;
  setLedColor(&RED);
  sendResponseHeaders(writer, 200, "", "");
}

void meetingEnded(BufferedResponseWriter& writer) {
  in_meeting = false;
  setLedColor(led_color_from_schedule);
  sendResponseHeaders(writer, 200, "", "");
}

void setLedColor(LedColor *new_color) {
  if (current_led_color != new_color) {
    if (!current_led_color->off) {
      analogWrite(current_led_color->pin, 0);
    }
    if (!new_color->off) {
      analogWrite(new_color->pin, new_color->brightness);
    }
  }
  current_led_color = new_color;
}

void sendResponseHeaders(BufferedResponseWriter& writer, int response_code, const char *extra_headers, const char *content_type) {
  writer.write("HTTP/1.1 ");
  if (response_code < 999) {
    char response_code_str[4];
    itoa(response_code, response_code_str, 10);
    writer.write(response_code_str);
  } else {
    writer.write("200");
  }
  writer.write(" OK");
  writer.write("\n");
  if (strlen(content_type) > 0) {
    writer.write("Content-Type: ");
    writer.write(content_type);
    writer.write("\n");
  }
  writer.write("Access-Control-Allow-Origin: *\n");
  if (strlen(extra_headers) > 0) {
    writer.write(extra_headers);
    writer.write("\n");
  }
  writer.write("Connection: close\n");
  writer.write("\n");
}

void setSchedule(StringView query, BufferedResponseWriter& writer) {
  Spliterator spliterator = query.split("&");
  while (spliterator.next()) {
    StringView queryParamater = spliterator.current();
    int equalsIndex = queryParamater.indexOf("=");
    if (equalsIndex > 0) {
      StringView key = queryParamater.substring(0, equalsIndex);
      StringView value = queryParamater.substring(equalsIndex + 1);
      if (key.equals("meetings")) {
        parseMeetings(value);
      } else if (key.equals("ooo")) {
        parseOoo(value);
      }
    }
  }
  sendResponseHeaders(writer, 200, "", "");
}

int sort_time_ranges(const void *cmp1, const void *cmp2)
{
  Time* a = ((TimeRange *) cmp1)->getStart();
  Time* b = ((TimeRange *) cmp2)->getStart();
  return a > b ? 1 : (a < b ? -1 : 0);
  return a->getTime() > b->getTime() ? 1 : (a->getTime() < b->getTime() ? -1 : 0);
}

void parseMeetings(StringView meetingsList) {
  Spliterator spliterator = meetingsList.split(",");
  int meeting_index = 0;
  while (spliterator.next() && meeting_index < MAX_MEETING_COUNT) {
    StringView meeting = spliterator.current();
    int dashIndex = meeting.indexOf("-");
    if (dashIndex > 0) {
      Time start = Time(meeting.substring(0, dashIndex).toLong());
      Time end = Time(meeting.substring(dashIndex + 1).toLong());
      Time now = Time::now();
      if (start.getTime() == 0 || end.getTime() == 0) {
        Serial.write("Unable to parse ");
        meeting.writeToSerial();
        Serial.println(" into numbers");
      } else if (start.isAfter(end)) {
        Serial.write("Error parsing ");
        meeting.writeToSerial();
        Serial.println(" start > end");
      } else if (start.getDate() < now.getDate() - 1) {
        Serial.write("Error parsing ");
        meeting.writeToSerial();
        Serial.println(" start more than 1 day ago");
      } else if (end.getDate() > start.getDate() + 6) {
        Serial.write("Error parsing ");
        meeting.writeToSerial();
        Serial.println(" start more than 6 days in the future");
      } else {
        meetings[meeting_index++] = TimeRange(start, end);
      }
    }
  }
  for (int i = meeting_index; i < MAX_MEETING_COUNT; i++) {
    meetings[i] = TimeRange();
  }
  if (meeting_index > 0) {
    qsort(meetings, meeting_index-1, sizeof(meetings[0]), sort_time_ranges);
  }
}

void parseOoo(StringView oooList) {
  Spliterator spliterator = oooList.split(",");
  int ooo_index = 0;
  while (spliterator.next() && ooo_index < MAX_OOO_COUNT) {
    StringView ooo = spliterator.current();
    int dashIndex = ooo.indexOf("-");
    if (dashIndex > 0) {
      unsigned long start = ooo.substring(0, dashIndex).toLong();
      unsigned long end = ooo.substring(dashIndex + 1).toLong();
      if (start < end) {
        ooos[ooo_index++] = TimeRange(start, end);
      }
    }
  }
  for (int i = ooo_index; i < MAX_OOO_COUNT; i++) {
    ooos[i] = TimeRange();
  }
}


void sendStatus(BufferedResponseWriter& writer) {
  Time now = Time::now();
  
  sendResponseHeaders(writer, 200, "", "text/html");
  writer.write("<!DOCTYPE HTML>");
  writer.write("<html>");

  writer.write("<head>");
  writer.write("<link rel=\"icon\" href=\"favicon.svg\">");
  writer.write("<link rel=\"apple-touch-icon\" href=\"favicon.svg\">");
  writer.write("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  writer.write("<link rel=\"stylesheet\" href=\"style.css\">");
  writer.write("</head>");

  writer.write("<body>");
  writer.write("<img src=\"");
  writer.write(current_led_color->svg);
  writer.write("\" width=200 height=200/>");
  writer.write("<h1 class=\"center\">");
  writer.write(current_led_color->message);
  writer.write("</h1>");

  writer.write("<div class=\"table\">");
  writer.write("<div class=\"cell header\"><p class=\"bold center\">Today's Meetings</p></div>");
  writeMeetingsForDate(writer, now.getDate());
  writer.write("<div class=\"cell header\"><p class=\"bold center\">Tomorrow's Meetings</p></div>");
  writeMeetingsForDate(writer, now.getDate() + 1);
  writer.write("</div>");

  writer.write("</body></html>");
}

void writeMeetingsForDate(BufferedResponseWriter& writer, int date) {
  boolean meetingAdded = false;
  for (int i = 0; i < MAX_MEETING_COUNT; i++) {
    TimeRange *meeting = &meetings[i];
    if (meeting->isEmpty()) {
      if (!meetingAdded) {
        writer.write("<div class=\"cell\"><p class=\"meeting center\">None</p></div>");
      }
      break;
    }
    if (meeting->getStart()->getDate() == date) {
      meetingAdded = true;
      writer.write("<div class=\"cell\"><p class=\"meeting center\">");
      meeting->prettyPrint(writer);
      writer.write("</p></div>");
    }
  }
}

void updateCurrentMeeting() {
  current_meeting = TimeRange();
  next_meeting = TimeRange();
  Time now = Time::now();
  for (int i = 0; i < MAX_MEETING_COUNT; i++) {
    TimeRange *meeting = &meetings[i];
    if (meeting->isEmpty()) {
      break;
    }
    if (meeting->isCurrentlyHappening(now)) {
      current_meeting = *meeting;
    } else if (next_meeting.isEmpty()) {
      if (meeting->isInTheFuture(now)) {
        next_meeting = *meeting;
      }
    }
  }

  currently_ooo = false;
  for (int i = 0; i < MAX_OOO_COUNT; i++) {
    TimeRange *ooo = &ooos[i];
    if (ooo->isCurrentlyHappening(now)) {
      currently_ooo = true;
      break;
    }
    if (ooo->isEmpty()) {
      break;
    }
  }
}

void updateLedBasedOnSchedule() {
  if (time_client.isTimeSet()) {
    Time now = Time::now();
    int day = now.getDay();
    int hour = now.getHour();
    if (day == SATURDAY || day == SUNDAY || hour < 7 || hour > 19 || currently_ooo) {
      led_color_from_schedule = &ALL_OFF;
    } else {
      if (!current_meeting.isEmpty() || (!next_meeting.isEmpty() && now.getTime() + 120 > next_meeting.getStart()->getTime())) {
        led_color_from_schedule = &YELLOW;
      } else {
        led_color_from_schedule = &GREEN;
      }
    }
    if (!in_meeting) {
      setLedColor(led_color_from_schedule);
    }
  }
}
