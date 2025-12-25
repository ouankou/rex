void sendMessage();
void sendMessage_forward() {
  // legacy frontend BUG: For C langauge mode this will be "return;" (missing
  // the call to "sendMessage()")
  return sendMessage();
}
