#pragma once

template <typename T> T rex_inline_definition_type(T input) {
  struct RexInlineDefinition {
    RexInlineDefinition() : value(0) {}
    ~RexInlineDefinition() { ++value; }
    int value;
  } const instance;
  return input;
}
