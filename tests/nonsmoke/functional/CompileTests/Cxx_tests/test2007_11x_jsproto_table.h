/* Share the local specimen data without depending on an external jsproto.tbl.
 */
#ifndef TEST2007_11X_JSPROTO_TABLE_H
#define TEST2007_11X_JSPROTO_TABLE_H

// clang-format off
#define TEST2007_11X_FOR_EACH_JS_PROTO(JS_PROTO)                               \
  JS_PROTO(Null,                   0,     js_InitNullClass)                    \
  JS_PROTO(Object,                 1,     js_InitFunctionAndObjectClasses)     \
  JS_PROTO(Function,               2,     js_InitFunctionAndObjectClasses)     \
  JS_PROTO(Array,                  3,     js_InitArrayClass)                   \
  JS_PROTO(Boolean,                4,     js_InitBooleanClass)                 \
  JS_PROTO(Call,                   5,     js_InitCallClass)                    \
  JS_PROTO(Date,                   6,     js_InitDateClass)                    \
  JS_PROTO(Math,                   7,     js_InitMathClass)                    \
  JS_PROTO(Number,                 8,     js_InitNumberClass)                  \
  JS_PROTO(String,                 9,     js_InitStringClass)                  \
  JS_PROTO(RegExp,                10,     js_InitRegExpClass)                  \
  JS_PROTO(Script,                11,     SCRIPT_INIT)                         \
  JS_PROTO(XML,                   12,     XML_INIT)                            \
  JS_PROTO(Namespace,             13,     NAMESPACE_INIT)                      \
  JS_PROTO(QName,                 14,     QNAME_INIT)                          \
  JS_PROTO(AnyName,               15,     ANYNAME_INIT)                        \
  JS_PROTO(AttributeName,         16,     ATTRIBUTE_INIT)                      \
  JS_PROTO(Error,                 17,     js_InitExceptionClasses)             \
  JS_PROTO(InternalError,         18,     js_InitExceptionClasses)             \
  JS_PROTO(EvalError,             19,     js_InitExceptionClasses)             \
  JS_PROTO(RangeError,            20,     js_InitExceptionClasses)             \
  JS_PROTO(ReferenceError,        21,     js_InitExceptionClasses)             \
  JS_PROTO(SyntaxError,           22,     js_InitExceptionClasses)             \
  JS_PROTO(TypeError,             23,     js_InitExceptionClasses)             \
  JS_PROTO(URIError,              24,     js_InitExceptionClasses)             \
  JS_PROTO(Generator,             25,     GENERATOR_INIT)                      \
  JS_PROTO(Iterator,              26,     js_InitIteratorClasses)              \
  JS_PROTO(StopIteration,         27,     js_InitIteratorClasses)              \
  JS_PROTO(UnusedProto28,         28,     js_InitNullClass)                    \
  JS_PROTO(File,                  29,     FILE_INIT)                           \
  JS_PROTO(Block,                 30,     js_InitBlockClass)
// clang-format on

#endif
