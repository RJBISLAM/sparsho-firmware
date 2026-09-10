// Thin Arduino IDE wrapper around the real firmware (main.c, renamed to
// sparsho_run so it doesn't collide with Arduino's own main()).
// sparsho_run() never returns — that's intentional, it matches the
// firmware's own infinite for(;;) loop — so loop() is simply never reached.
extern "C" void sparsho_run(void);

void setup() {
  sparsho_run();
}

void loop() {
}
