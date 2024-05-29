class Spliterator;

char parsing_buffer[21];

class StringView {
  public:
    StringView();
    StringView(StringView &s);
    StringView(StringView *s);
    StringView(const char *s, int len);
    int indexOf(const char *needle);
    StringView substring(int start);
    StringView substring(int start, int len);
    bool equals(const char *s);
    int length();
    const char* buff();
    Spliterator split(const char *s);
    unsigned long toLong();
    int toInt();
    void writeToSerial();
  private:
    const char *_s;
    int _len;
};

StringView::StringView()
  : _s(""), _len(0) {}

StringView::StringView(StringView &s)
  : _s(s._s), _len(s._len) {}

StringView::StringView(StringView *s)
  : _s(s->_s), _len(s->_len) {}

StringView::StringView(const char *s, int len)
  : _s(s), _len(len) {}

int StringView::indexOf(const char *needle) {
  char *found;
  found = strstr(_s, needle);
  if (found == NULL || found > _s + _len) {
    return -1;
  }
  return found - _s;
}

StringView StringView::substring(int start) {
  return substring(start, _len - start);
}

StringView StringView::substring(int start, int len) {
  return StringView(_s + start, len);
}

int StringView::length() {
  return _len;
}

bool StringView::equals(const char *s) {
  int len = strlen(s);
  return len == _len && strncmp(_s, s, len) == 0;
}

const char* StringView::buff() {
  return _s;
}

unsigned long StringView::toLong() {
  memset(parsing_buffer, 0, sizeof(parsing_buffer));
  int parseLength = min(_len, sizeof(parsing_buffer) - 1);
  memcpy(parsing_buffer, _s, parseLength);
  return atol(parsing_buffer);
}

int StringView::toInt() {
  memset(parsing_buffer, 0, sizeof(parsing_buffer));
  int parseLength = min(_len, sizeof(parsing_buffer) - 1);
  memcpy(parsing_buffer, _s, parseLength);
  return atol(parsing_buffer);
}

void StringView::writeToSerial() {
  Serial.write(_s, _len);
}

class Spliterator {
  public:
    Spliterator(StringView s, const char *delimiter);
    boolean next();
    StringView current();
  private:
    StringView _current;
    StringView _rest;
    const char *_delimiter;
    boolean _finished;
};

Spliterator::Spliterator(StringView s, const char *delimiter) :
  _rest(s),
  _delimiter(delimiter),
  _finished(false) {}

boolean Spliterator::next() {
  if (_finished) {
    return false;
  }
  int index = _rest.indexOf(_delimiter);
  if (index >= 0) {
    _current = _rest.substring(0, index);
    _rest = _rest.substring(index + 1);
  } else if (!_finished) {
    _finished = true;
    _current = _rest;
    _rest = StringView();
  }
  return true;
}

StringView Spliterator::current() {
  return _current;
}

Spliterator StringView::split(const char *delimiter) {
  return Spliterator(this, delimiter);
}

class BufferedResponseWriter {
  public:
    BufferedResponseWriter(WiFiClient *client, char *buff, int len) : _client(client), _buff(buff), _len(len), _written(0) {}
    void write(const char *s) {
      write(s, strlen(s));
    }

    void write(const char *s, int len) {
      writeWithFuncion(s, len, writeFunc);
    }

    void write_P(const char *s, int len) {
      writeWithFuncion(s, len, write_PFunc);
    }

    void write(int i) {
      memset(parsing_buffer, 0, sizeof(parsing_buffer));
      itoa(i, parsing_buffer, 10);
      write(parsing_buffer);
    }

    void write(unsigned long l) {
      memset(parsing_buffer, 0, sizeof(parsing_buffer));
      ltoa(l, parsing_buffer, 10);
      write(parsing_buffer);
      write("L");
    }

    void write(bool b) {
      write(b ? "true" : "false");
    }

    void flush() {
      _client->write(_buff, _written);
      _written = 0;
    }
  private:
    char *_buff;
    int _written;
    const int _len;
    WiFiClient *_client;

    static void writeFunc(char *dest, const char *source, int len) {
      memcpy(dest, source, len);
    }

    static void write_PFunc(char *dest, const char *source, int len) {
      memcpy_P(dest, source, len);
    }

    void writeWithFuncion(const char *s, int len, void (*memcpy_func)(char *, const char *s, int)) {
      int written = 0;
      while (written < len) {
        int can_write = min(len - written, _len - _written);
        memcpy_func(_buff + _written, s + written, can_write);
        _written += can_write;
        written += can_write;
        if (_written == _len) {
          flush();
        }
      }
    }
};
