#include <compare>

class PersonInFamilyTree { // ...
  int id;
  int parent_id;

public:
  PersonInFamilyTree(int id_value = 0, int parent_id_value = -1)
      : id(id_value), parent_id(parent_id_value) {}

  bool is_the_same_person_as(const PersonInFamilyTree &that) const {
    return id == that.id;
  }

  bool is_transitive_child_of(const PersonInFamilyTree &that) const {
    return parent_id == that.id && id != that.id;
  }

  std::partial_ordering operator<=>(const PersonInFamilyTree &that) const {
    if (this->is_the_same_person_as(that))
      return std::partial_ordering::equivalent;
    if (this->is_transitive_child_of(that))
      return std::partial_ordering::less;
    if (that.is_transitive_child_of(*this))
      return std::partial_ordering::greater;
    return std::partial_ordering::unordered;
  }

  // ... non-comparison functions ...
};

// DQ (7/21/2020): Moved function calls into a function.
void foobar1() {
  // compiler generates all four relational operators
  PersonInFamilyTree per1, per2;

  if (per1 < per2) { /*...*/
  } // ok, per2 is an ancestor of per1
  else if (per1 > per2) { /*...*/
  } // ok, per1 is an ancestor of per2
  else if (std::is_eq(per1 <=> per2)) { /*...*/
  } // ok, per1 is per2
  else { /*...*/
  } // per1 and per2 are unrelated

  if (per1 <= per2) { /*...*/
  } // ok, per2 is per1 or an ancestor of per1
  if (per1 >= per2) { /*...*/
  } // ok, per1 is per2 or an ancestor of per2
  if (std::is_neq(per1 <=> per2)) { /*...*/
  } // ok, per1 is not per2
}
