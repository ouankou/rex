int rex_frontend_preprocessor_scope_gap() {
#define REX_FRONTEND_INCREMENT(value) ((value) + 1)
  return REX_FRONTEND_INCREMENT(41);
}

int rex_frontend_preprocessor_expression_scope_gap() {
  const int result = []() {
#define REX_FRONTEND_LAMBDA_INCREMENT(value) ((value) + 1)
    return REX_FRONTEND_LAMBDA_INCREMENT(41);
  }();
  return result;
}

struct RexFrontendPreprocessorCtorScopeGap {
  int value;

  RexFrontendPreprocessorCtorScopeGap()
      :
#define REX_FRONTEND_CTOR_INCREMENT(value) ((value) + 2)
        value(REX_FRONTEND_CTOR_INCREMENT(
            40) /* REX_FRONTEND_CTOR_EXPRESSION_COMMENT */) {
  }
};

#define REX_FRONTEND_EMPTY_CTOR_BODY
struct RexFrontendPreprocessorEmptyCtorGap {
  RexFrontendPreprocessorEmptyCtorGap()
#ifdef REX_FRONTEND_EMPTY_CTOR_BODY
      {}
#else
      ;
#endif
};

#define REX_FRONTEND_CONDITIONAL_STORAGE int
int rex_frontend_preprocessor_semantic_definition_gap() {
#ifdef REX_FRONTEND_CONDITIONAL_STORAGE
  register
#endif
      REX_FRONTEND_CONDITIONAL_STORAGE conditional_value = 7;
  return conditional_value;
}

int rex_frontend_preprocessor_conditional_argument_target(int first,
                                                          int second = 0) {
  return first + second;
}

int rex_frontend_preprocessor_conditional_argument_gap() {
  return rex_frontend_preprocessor_conditional_argument_target(40
#if REX_FRONTEND_CONDITIONAL_ARGUMENT_ENABLED
                                                               ,
                                                               2
#endif
  );
}

struct RexFrontendPreprocessorConditionalCtorGap {
  int first;
  int second;

  RexFrontendPreprocessorConditionalCtorGap()
      : first(40)
#if REX_FRONTEND_CONDITIONAL_CTOR_ENABLED
        ,
        second(2)
#endif
  {
  }
};
