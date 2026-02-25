// From: Xen: static void tapdisk_control_handle_request(event_id_t id, char
// mode, void *private)
void foobar() {
  switch (0) {
  case 42:
    // Keep this as valid C while preserving the same control-flow shape.
    return;
  }
}
