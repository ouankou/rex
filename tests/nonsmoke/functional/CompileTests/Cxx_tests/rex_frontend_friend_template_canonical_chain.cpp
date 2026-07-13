namespace rex_frontend_friend_template_canonical_chain {
template <class Element, class Allocator> class vector {};

class bit_reference {
  template <class, class> friend class vector;
};
} // namespace rex_frontend_friend_template_canonical_chain
