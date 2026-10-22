#include "AnnotDescriptors.h"

#include "AstInterface.h"

#include "ROSE_ASSERT.h"

#include <list>

#include <sstream>

using namespace std;

template <class Container, class Member, char sep, char left, char right>
bool ReadContainer<Container, Member, sep, left, right>::read(Container &c,
                                                              istream &in) {
  if (peek_id(in) == "none") {
    read_id(in, "none");
  } else {
    if (left != 0)
      read_ch(in, left);

    if (peek_id(in) == "none")
      read_id(in, "none");
    else if (peek_ch(in) != right) {
      ReadContainerWrap<Member, Container> op(c);
      if (!read_list(in, op, sep))
        throw ReadError("annotation container has no first item");
    }
    if (right != 0)
      read_ch(in, right);
  }
  return true;
}

template <class Container, char sep, char left, char right>
void WriteContainer<Container, sep, left, right>::write(const Container &c,
                                                        ostream &out) {
  out << left;
  bool first = true;

  for (typename Container::const_iterator p = c.begin(); p != c.end(); ++p) {
    if (!first)
      out << sep;

    (*p).write(out);

    first = false;
  }

  out << right;
}

template <class First, class Second, char sep>
bool CollectPair<First, Second, sep>::read(istream &in) {
  if (!this->first.read(in))
    return false;

  if (sep != 0) {
    read_ch(in, sep);
  }
  if (!this->second.read(in))
    throw ReadError("annotation pair has no second item");

  return true;
}

template <class First, class Second, char sep, char sel>
bool SelectPair<First, Second, sep, sel>::read(istream &in) {
  if (!this->first.read(in))
    return false;
  char c = peek_ch(in);
  if (c == sel) {
    if (!this->second.read(in))
      throw ReadError("annotation pair has no selected item");
  } else if (c == sep) {
    read_ch(in, sep);
    if (!this->second.read(in))
      throw ReadError("annotation pair has no second item");
  }
  return true;
}

template <class First, class Second, char sep>
void CollectPair<First, Second, sep>::write(ostream &out) const {
  this->first.write(out);
  if (sep != 0)
    out << sep;
  else
    out << ' ';
  this->second.write(out);
}

template <class First, class Second, char sep, char sel>
void SelectPair<First, Second, sep, sel>::write(ostream &out) const {
  this->first.write(out);
  out << sep;
  this->second.write(out);
}

template <class Descriptor, char left, char right>
bool CloseDescriptor<Descriptor, left, right>::read(istream &in) {
  read_ch(in, left);
  if (peek_ch(in) != right && !Descriptor::read(in))
    throw ReadError("annotation delimiter has no content");
  read_ch(in, right);
  return true;
}

template <class Descriptor, char left, char right>
void CloseDescriptor<Descriptor, left, right>::write(ostream &out) const {
  out << left;
  Descriptor::write(out);
  out << right;
}

#ifndef TEMPLATE_ONLY
//! Read in a class name, could be a derived class with a qualified name
// "class" is already processed by ReadAnnotation::read() before this function
// is called.
bool TypeDescriptor::read(istream &in) {
  if (!StringDescriptor::read(in)) // read the first string
    return false;
  if (get_name() == "const" || get_name() == "struct") {
    std::string type_name = read_id(in);
    if (type_name.empty())
      throw ReadError("annotation type modifier has no type name");
    get_name() += type_name;
  }
  char c = peek_ch(in);
  while (c == ':' && peek_next(in, 2) == "::") {
    read_ch(in, ':');
    read_ch(in, ':');
    std::string segment = read_id(in);
    if (segment.empty())
      throw ReadError("annotation type has an empty qualified segment");
    get_name() += "::" + segment;
    c = peek_ch(in);
  }
  // The type name could be reference or pointer type?
  if (c == '&' || c == '*') {
    read_ch(in, c);
    get_name().push_back(c);
  }
  return true;
}

//! Read in a variable name, supporting qualified names
bool NameDescriptor::read(istream &in) {
  if (!StringDescriptor::read(in)) // read the first string
    return false;
  char c = peek_ch(in);
  while (c == ':' && peek_next(in, 2) == "::") {
    read_ch(in, ':');
    read_ch(in, ':');
    std::string segment = read_id(in);
    if (segment.empty())
      throw ReadError("annotation name has an empty qualified segment");
    get_name() += "::" + segment;
    c = peek_ch(in);
  }
  return true;
}

//! Read in a variable name, supporting qualified names
bool NameDescriptor::write(std::ostream &out) const {
  std::string content = get_name();
  auto output_char = [&out](const char c) {
    if (std::isalnum(c) || c == ':') {
      out << c;
    } else if (c == '_' || c == ',' || c == '(' || c == ')') {
      out << '_';
    } else if (c == '&') {
      out << "_ref_";
    } else if (c == ' ' || c == '\t' || c == '\n') {
      /* skip empty space */
    } else {
      out << c;
    }
  };
  for (const char c : content) {
    output_char(c);
  }
  return true;
}

#endif
