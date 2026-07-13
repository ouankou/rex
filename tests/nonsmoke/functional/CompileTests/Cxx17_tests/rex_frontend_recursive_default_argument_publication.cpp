struct rex_recursive_default_argument_owner {
  rex_recursive_default_argument_owner(
      int = (rex_recursive_default_argument_owner(5), 0)) noexcept;
};
