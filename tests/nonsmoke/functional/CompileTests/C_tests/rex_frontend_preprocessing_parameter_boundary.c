#define REX_PREPROCESSING_PARAMETER_ENABLED 1

void rex_preprocessing_parameter_directive(
#if REX_PREPROCESSING_PARAMETER_ENABLED
    int rex_preprocessing_parameter
#endif
) {
}

#define REX_PREPROCESSING_EMPTY_PARAMETER_ENABLED 1

void rex_preprocessing_empty_parameter_directive(
#if REX_PREPROCESSING_EMPTY_PARAMETER_ENABLED
#endif
);

void rex_preprocessing_conditional_trailing_parameter(
    int rex_preprocessing_required_leading_parameter
#if REX_PREPROCESSING_TRAILING_PARAMETER_ENABLED
    ,
    int rex_preprocessing_trailing_parameter
#endif
);

void rex_preprocessing_conditional_leading_parameter(
#if REX_PREPROCESSING_LEADING_PARAMETER_ENABLED
    int rex_preprocessing_leading_parameter,
#endif
    int rex_preprocessing_required_trailing_parameter);
