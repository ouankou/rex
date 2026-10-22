void rex_ast_json_original();
void rex_ast_json_target(int value[3] = nullptr);

void rex_ast_json_original() {}

template <class T> void rex_ast_json_friend_parameter(T *);
class RexAstJsonFriendParameterOwner {
  friend void rex_ast_json_friend_parameter<int>(int values[3]);
};
