class rex_outliner_friend_linkage_owner {
protected:
  int value_ = 7;

public:
  int read() const {
    int result;
#pragma rose_outline
    result = value_;
    return result;
  }
};
