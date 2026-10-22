struct RexVfaBase {
  virtual void rex_vfa_pointer_dispatch() {}
  virtual void rex_vfa_single_dispatch() {}
};

struct RexVfaExpected : RexVfaBase {
  void rex_vfa_pointer_dispatch() override {}
};

struct RexVfaOther : RexVfaBase {
  void rex_vfa_pointer_dispatch() override {}
};

struct RexVfaExpectedChild : RexVfaExpected {
  void rex_vfa_pointer_dispatch() override {}
};

RexVfaExpected *rex_vfa_pointer_factory() { return new RexVfaExpected; }
RexVfaBase *rex_vfa_base_factory() { return new RexVfaExpected; }
RexVfaOther *rex_vfa_other_factory() { return new RexVfaOther; }

using RexVfaAddress = __UINTPTR_TYPE__;
RexVfaAddress rex_vfa_external_address();
RexVfaAddress rex_vfa_integer_factory() {
  return reinterpret_cast<RexVfaAddress>(rex_vfa_base_factory());
}
RexVfaAddress rex_vfa_integer_forward(RexVfaAddress value) { return value; }
void rex_vfa_narrow_integer_sink(RexVfaAddress) {}
void rex_vfa_boolean_integer_sink(RexVfaAddress) {}
RexVfaBase *rex_vfa_integer_relay() {
  return reinterpret_cast<RexVfaBase *>(
      rex_vfa_integer_forward(rex_vfa_integer_factory()));
}
void rex_vfa_unknown_integer(bool choose) {
  RexVfaAddress value = reinterpret_cast<RexVfaAddress>(rex_vfa_base_factory());
  if (choose)
    value = rex_vfa_external_address();
  auto *receiver = reinterpret_cast<RexVfaBase *>(value);
  receiver->rex_vfa_pointer_dispatch();
}
void rex_vfa_reassigned_integer() {
  RexVfaAddress value = reinterpret_cast<RexVfaAddress>(rex_vfa_base_factory());
  value = reinterpret_cast<RexVfaAddress>(rex_vfa_other_factory());
  auto *receiver = reinterpret_cast<RexVfaBase *>(value);
  receiver->rex_vfa_pointer_dispatch();
}

RexVfaBase *rex_vfa_forward() { return rex_vfa_pointer_factory(); }
RexVfaBase *rex_vfa_relay() {
  return static_cast<RexVfaBase *>(static_cast<void *>(rex_vfa_forward()));
}
RexVfaBase *rex_vfa_select(bool choose) {
  if (choose)
    return rex_vfa_pointer_factory();
  return rex_vfa_other_factory();
}
RexVfaBase *rex_vfa_conditional(bool choose) {
  return choose ? static_cast<RexVfaBase *>(rex_vfa_pointer_factory())
                : static_cast<RexVfaBase *>(rex_vfa_other_factory());
}
RexVfaBase *rex_vfa_gnu(RexVfaBase *value) {
  return value ?: rex_vfa_other_factory();
}
RexVfaBase *rex_vfa_null_factory() { return nullptr; }
RexVfaBase *rex_vfa_zero_factory() { return 0; }
void rex_vfa_null_consumer(RexVfaBase *) {}
RexVfaBase *rex_vfa_comma() {
  return (rex_vfa_null_consumer(nullptr), rex_vfa_pointer_factory());
}
void rex_vfa_selected_call(bool choose) {
  RexVfaBase *value = rex_vfa_select(choose);
  value->rex_vfa_pointer_dispatch();
  value = rex_vfa_conditional(choose);
  value->rex_vfa_pointer_dispatch();
  value = rex_vfa_gnu(choose ? nullptr : rex_vfa_pointer_factory());
  value->rex_vfa_pointer_dispatch();
}

void rex_vfa_consume(RexVfaBase *value) { value->rex_vfa_pointer_dispatch(); }

struct RexVfaSink {
  explicit RexVfaSink(RexVfaBase *value) { value->rex_vfa_pointer_dispatch(); }
};

RexVfaBase *&rex_vfa_reference(RexVfaBase *&value) { return value; }
void rex_vfa_reference_consume(RexVfaBase *&value) {
  value->rex_vfa_pointer_dispatch();
}

void rex_vfa_overwrite(RexVfaBase *&value) {
  value = rex_vfa_pointer_factory();
}
void rex_vfa_overwrite_both(RexVfaBase *&value, bool choose) {
  if (choose)
    value = rex_vfa_pointer_factory();
  else
    value = rex_vfa_pointer_factory();
}
void rex_vfa_maybe_overwrite(RexVfaBase *&value, bool choose) {
  if (choose)
    value = rex_vfa_pointer_factory();
}
void rex_vfa_maybe_write(bool choose) {
  RexVfaBase *value = rex_vfa_other_factory();
  rex_vfa_maybe_overwrite(value, choose);
  value->rex_vfa_pointer_dispatch();
}

struct RexVfaUpdater {
  virtual void update(RexVfaBase *&value) = 0;
};
struct RexVfaReplacingUpdater : RexVfaUpdater {
  void update(RexVfaBase *&value) override {
    value = rex_vfa_pointer_factory();
  }
};
struct RexVfaKeepingUpdater : RexVfaUpdater {
  void update(RexVfaBase *&) override {}
};
void rex_vfa_choice_write(bool choose) {
  RexVfaUpdater *updater =
      choose ? static_cast<RexVfaUpdater *>(new RexVfaReplacingUpdater)
             : static_cast<RexVfaUpdater *>(new RexVfaKeepingUpdater);
  RexVfaBase *value = rex_vfa_other_factory();
  updater->update(value);
  value->rex_vfa_pointer_dispatch();
}

void rex_vfa_null_dispatch() {
  RexVfaBase *receiver = nullptr;
  receiver->rex_vfa_pointer_dispatch();
}

void rex_vfa_unreachable_dispatch() {
  RexVfaBase *receiver = rex_vfa_pointer_factory();
  receiver->rex_vfa_pointer_dispatch();
}

void rex_vfa_null_single_dispatch() {
  RexVfaBase *receiver = nullptr;
  receiver->rex_vfa_single_dispatch();
}

void rex_vfa_unreachable_single_dispatch() {
  RexVfaBase *receiver = rex_vfa_pointer_factory();
  receiver->rex_vfa_single_dispatch();
}

int main(int argc, char **) {
  RexVfaBase *implicit = rex_vfa_pointer_factory();
  implicit->rex_vfa_pointer_dispatch();
  implicit->rex_vfa_single_dispatch();
  implicit->RexVfaBase::rex_vfa_pointer_dispatch();
  RexVfaBase *same = static_cast<RexVfaBase *>(rex_vfa_base_factory());
  same->rex_vfa_pointer_dispatch();
  RexVfaBase *erased =
      static_cast<RexVfaBase *>(static_cast<void *>(rex_vfa_base_factory()));
  erased->rex_vfa_pointer_dispatch();
  RexVfaBase *assigned;
  assigned = rex_vfa_pointer_factory();
  assigned->rex_vfa_pointer_dispatch();
  rex_vfa_consume(rex_vfa_pointer_factory());
  rex_vfa_consume(rex_vfa_other_factory());
  RexVfaSink sink(rex_vfa_pointer_factory());
  RexVfaBase *forwarded = rex_vfa_comma();
  forwarded->rex_vfa_pointer_dispatch();
  RexVfaBase *overwritten = new RexVfaOther;
  overwritten = rex_vfa_pointer_factory();
  overwritten->rex_vfa_pointer_dispatch();
  rex_vfa_relay()->rex_vfa_pointer_dispatch();
  rex_vfa_selected_call(argc > 1);
  rex_vfa_consume(rex_vfa_relay());
  RexVfaSink forwarded_sink(rex_vfa_relay());
  rex_vfa_null_consumer(rex_vfa_null_factory());
  rex_vfa_null_consumer(rex_vfa_zero_factory());
  RexVfaBase *through_reference = new RexVfaOther;
  RexVfaBase *&reference = through_reference;
  reference = rex_vfa_pointer_factory();
  reference->rex_vfa_pointer_dispatch();
  through_reference = new RexVfaOther;
  rex_vfa_reference(through_reference) = rex_vfa_pointer_factory();
  through_reference->rex_vfa_pointer_dispatch();
  rex_vfa_reference_consume(through_reference);
  RexVfaBase &object_reference = *rex_vfa_pointer_factory();
  object_reference.rex_vfa_pointer_dispatch();
  rex_vfa_reference(through_reference)->rex_vfa_pointer_dispatch();
  RexVfaBase *replaced = rex_vfa_other_factory();
  rex_vfa_overwrite(replaced);
  replaced->rex_vfa_pointer_dispatch();
  RexVfaBase *both = rex_vfa_other_factory();
  rex_vfa_overwrite_both(both, argc > 1);
  both->rex_vfa_pointer_dispatch();
  rex_vfa_maybe_write(argc > 1);
  rex_vfa_choice_write(argc > 1);
  RexVfaAddress address =
      reinterpret_cast<RexVfaAddress>(rex_vfa_base_factory());
  auto *integer_roundtrip = reinterpret_cast<RexVfaBase *>(address);
  integer_roundtrip->rex_vfa_pointer_dispatch();
  auto *checked_roundtrip = dynamic_cast<RexVfaExpected *>(integer_roundtrip);
  checked_roundtrip->rex_vfa_pointer_dispatch();
  unsigned __int128 extended_address = address;
  auto *wide_roundtrip = reinterpret_cast<RexVfaBase *>(
      static_cast<RexVfaAddress>(extended_address));
  wide_roundtrip->rex_vfa_pointer_dispatch();
  rex_vfa_integer_relay()->rex_vfa_pointer_dispatch();
  rex_vfa_unknown_integer(argc > 1);
  rex_vfa_reassigned_integer();
  unsigned char narrow_address = static_cast<unsigned char>(address);
  rex_vfa_narrow_integer_sink(narrow_address);
  bool present = static_cast<bool>(rex_vfa_base_factory());
  rex_vfa_boolean_integer_sink(present);
  if (argc < 0)
    rex_vfa_null_single_dispatch();
  if (argc == 0)
    rex_vfa_null_dispatch();
}
