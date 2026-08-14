#pragma once

template <class T>
concept rex_explicit_instantiation_operand =
    requires(T value) { value + value; } && (sizeof(T) > 1);

template <class T>
inline constexpr bool rex_explicit_instantiation_semantic_initializer =
    requires {
      requires rex_explicit_instantiation_operand<T> && (sizeof(T) < 32);
    };
