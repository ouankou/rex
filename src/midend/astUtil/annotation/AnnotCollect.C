

#include "AnnotCollect.h"

#include "CommandOptions.h"

using namespace std;

ReadAnnotation *ReadAnnotation::inst = 0;

ReadAnnotation *ReadAnnotation::get_inst() {
  if (inst == 0)
    inst = new ReadAnnotation();
  return inst;
}

void ReadAnnotation::release_inst() {
  delete inst;
  inst = nullptr;
}
//! Process command line and call read(istream & in)
void ReadAnnotation::read() {
  CmdOptions *inst = CmdOptions::GetInstance();
  std::vector<std::string>::const_iterator p = inst->begin();
  for (; p != inst->end(); p++) {
    if ((*p) == "-annot") {
      if (++p == inst->end())
        throw ReadError("-annot requires a filename");
      string p1 = *p;
      ifstream is(p1.c_str());
      if (!is.is_open()) {
        throw ReadError("Cannot find and open the annotation file " + p1);
      }
      read(is);
    }
  }
}
//! Read the annotation file and recognize type and operator annotation
//! collections
void ReadAnnotation::read(istream &in) {
  for (;;) {
    char next = peek_ch(in);
    if (next == 0 && in.eof())
      break;
    string buf = read_id(in);

    if (buf.empty())
      throw ReadError("expected class, operator, or variable annotation");

    if (buf == "class") {
      typeInfo.read(in);
    } else if (buf == "operator") {
      opInfo.read(in);
    } else if (buf == "variable") {
      varInfo.read(in);
    } else {
      ReadError m("non-recognizable annotation: \"" + buf + "\"");
      cerr << m.msg << endl;
      throw m;
    }
  }
}
