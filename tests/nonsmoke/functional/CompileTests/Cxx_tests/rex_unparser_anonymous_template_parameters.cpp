template <int> struct RexAnonymousNonType {};

template <template <class> class> struct RexAnonymousTemplateTemplate {};

template <class> struct RexTemplateArgument {};

RexAnonymousNonType<1> rex_anonymous_non_type;
RexAnonymousTemplateTemplate<RexTemplateArgument>
    rex_anonymous_template_template;
