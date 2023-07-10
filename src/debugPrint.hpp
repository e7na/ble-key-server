
#ifdef DEBUG
#define print(...) Serial.print(__VA_ARGS__)
#define println(...) Serial.println(__VA_ARGS__)
#define DECI_BUFFER_SIZE 1023
static char deciBuffer[DECI_BUFFER_SIZE];
#else
#define print(...)
#define println(...)
#endif

void deciPrint(const char *message, uint8_t *data) {
#ifdef DEBUG
  for (uint8_t i = 0; i < BLOCK_WIDTH; i++) {
    if (i > 0) {
      sprintf(deciBuffer, "%s, %d", deciBuffer, (uint8_t)data[i]);
    } else {
      sprintf(deciBuffer, "%d", (uint8_t)data[i]);
    }
  }
  print(message);
  println(deciBuffer);
#endif
}
