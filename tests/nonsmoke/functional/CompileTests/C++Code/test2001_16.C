enum Status { kOk = 16, kErr = 0 };

int value_or_default(Status s, int fallback = 16) {
  return s == kOk ? 16 : fallback;
}

int main() { return value_or_default(kOk) - 16; }
