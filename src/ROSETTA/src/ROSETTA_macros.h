#ifndef __ROSETTAMACROS_H__
#define __ROSETTAMACROS_H__

// MK: These header files contains macro definitions to be used
// by the setDataPrototype() member function of class AstNodeClass

#include "ROSE_ABORT.h"

#include "ROSE_ASSERT.h"

// Macro used to define terminals we want to object to be built on the heap so
// that it will not go out of scope, but we want a reference to the object
// instead of a pointer to the object to preserve the interface with existing
// code!
#define NEW_TERMINAL_MACRO(TERMINAL_NAME, TERMINAL_NAME_STRING,                \
                           TAG_NAME_STRING)                                    \
  AstNodeClass &TERMINAL_NAME = terminalConstructor(                           \
      TERMINAL_NAME_STRING, *this, TERMINAL_NAME_STRING, TAG_NAME_STRING);     \
  ROSE_ASSERT(TERMINAL_NAME.associatedGrammar != NULL);

// A new nonterminal should not be born a parent of any child
#define NEW_NONTERMINAL_MACRO(NONTERMINAL_NAME, NONTERMINAL_EXPRESSION,        \
                              NONTERMINAL_NAME_STRING, NONTERMINAL_TAG_STRING, \
                              NONTERMINAL_CAN_HAVE_INSTANCES)                  \
  AstNodeClass &NONTERMINAL_NAME =                                             \
      nonTerminalConstructor(NONTERMINAL_NAME_STRING, *this,                   \
                             NONTERMINAL_NAME_STRING, NONTERMINAL_TAG_STRING,  \
                             (SubclassListBuilder() | NONTERMINAL_EXPRESSION), \
                             NONTERMINAL_CAN_HAVE_INSTANCES);                  \
  ROSE_ASSERT(NONTERMINAL_NAME.associatedGrammar != NULL);

#endif
