#pragma once

template <class T>
concept rex_external_constraint =
    requires(T value) { value + value; } && (sizeof(T) > 1);
