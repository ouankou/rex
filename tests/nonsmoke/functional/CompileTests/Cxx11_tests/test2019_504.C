// Test from Pat Miller:

typedef float __m128 __attribute__((__vector_size__(16), __may_alias__));
typedef float __v4sf __attribute__((__vector_size__(16)));
extern __inline void
    __attribute__((__gnu_inline__, __always_inline__, __artificial__))
    _mm_store_ss(float *__P, __m128 __A) {
  *__P = ((__v4sf)__A)[0];
}

// Error:
// Case not handled! inner_kind = eok_vector_subscript
// Rose[FATAL]: assertion failed:
// Rose[FATAL]:
// /export/tmp.rose-mgr/jenkins/legacy_frontend/workspace/development-frontend-binary-RMC/COMPILER/gcc-5.4.0-default/FRONTEND_VERSION/5.0/src/frontend/CxxFrontend/legacy
// frontend/frontend_bridge/frontend_bridge.C:13263 Rose[FATAL]:
// TranslatedExpression
// Frontend_Translation::convert_expression(an_expr_node_ptr) Rose[FATAL]:
// required: false
