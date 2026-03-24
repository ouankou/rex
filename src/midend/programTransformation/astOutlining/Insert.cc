/**
 *  \file Transform/Insert.cc
 *
 *  \brief Inserts the outlined function declarations (i.e., including
 *  prototypes) and function calls.
 */
// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"

#include "sageBuilder.h"

#include <iostream>

#include <list>

#include <sstream>

#include <string>

#include "ASTtools.hh"

#include "Copy.hh"

#include "Outliner.hh"

#include "PreprocessingInfo.hh"

#include "RoseAst.h"

#include "StmtRewrite.hh"

// =====================================================================

typedef std::vector<SgFunctionDeclaration *> FuncDeclList_t;

// =====================================================================

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;
using namespace Outliner;

// =====================================================================

//! Creates a 'prototype' (forward declaration) for a function.
static SgFunctionDeclaration *
generatePrototype(const SgFunctionDeclaration *full_decl,
                  SgScopeStatement *scope, bool forceFreeFunctionScope) {
  if (!full_decl)
    return 0; // nothing to do

  if (SgTemplateFunctionDeclaration *template_decl =
          isSgTemplateFunctionDeclaration(
              const_cast<SgFunctionDeclaration *>(full_decl))) {
    SgFunctionType *funcType = template_decl->get_type();
    SgType *return_type = funcType->get_return_type();
    SgFunctionParameterList *paralist =
        deepCopy<SgFunctionParameterList>(template_decl->get_parameterList());
    SgTemplateParameterPtrList *template_params =
        new SgTemplateParameterPtrList();
    for (SgTemplateParameterPtrList::const_iterator it =
             template_decl->get_templateParameters().begin();
         it != template_decl->get_templateParameters().end(); ++it) {
      if (*it == NULL)
        continue;
      SgTemplateParameter *param_copy =
          isSgTemplateParameter(ASTtools::deepCopy(*it));
      ROSE_ASSERT(param_copy != NULL);
      template_params->push_back(param_copy);
    }

    SgTemplateFunctionDeclaration *proto =
        SageBuilder::buildNondefiningTemplateFunctionDeclaration(
            template_decl->get_name(), return_type, paralist, scope,
            template_params);
    ROSE_ASSERT(proto != NULL);
    for (SgTemplateParameterPtrList::iterator it =
             proto->get_templateParameters().begin();
         it != proto->get_templateParameters().end(); ++it) {
      if (*it != NULL)
        (*it)->set_parent(proto);
    }

    if (full_decl->get_functionModifier().isInline())
      proto->get_functionModifier().setInline();

    proto->set_linkage(full_decl->get_linkage());
    if (full_decl->get_declarationModifier().get_storageModifier().isExtern() ==
        true) {
      proto->get_declarationModifier().get_storageModifier().setExtern();
    }

    ROSE_ASSERT(proto->get_firstNondefiningDeclaration() != NULL);
    return proto;
  }

  // DQ (2/23/2009): Use this code instead.
  SgFunctionType *funcType = full_decl->get_type();
  SgType *return_type = funcType->get_return_type();
  SgFunctionParameterList *paralist =
      deepCopy<SgFunctionParameterList>(full_decl->get_parameterList());
  SgFunctionDeclaration *proto =
      SageBuilder::buildNondefiningFunctionDeclaration(
          full_decl->get_name(), return_type, paralist, scope, false, NULL,
          SgStorageModifier::e_default, forceFreeFunctionScope);
  ROSE_ASSERT(proto != NULL);

  // Inherit defining function's inline property: avoid linking error when
  // linking multiple .lib files with the outlined functions
  if (full_decl->get_functionModifier().isInline())
    proto->get_functionModifier().setInline();

  // Keep language linkage consistent with the defining declaration (e.g.,
  // outlined functions may be forced to `extern "C"`).
  proto->set_linkage(full_decl->get_linkage());
  if (full_decl->get_declarationModifier().get_storageModifier().isExtern() ==
      true) {
    proto->get_declarationModifier().get_storageModifier().setExtern();
  }

  // This should be the defining declaration (check it).
  ROSE_ASSERT(full_decl->get_definition() != NULL);

  // printf ("full_decl                            = %p = %s
  // \n",full_decl,full_decl->class_name().c_str()); printf
  // ("full_decl->get_definingDeclaration() = %p = %s
  // \n",full_decl->get_definingDeclaration(),full_decl->get_definingDeclaration()->class_name().c_str());
  ROSE_ASSERT(full_decl->get_definingDeclaration() == full_decl);

  // DQ (2/23/2009): This will result in a cross file edge if we have outlined
  // to a separate file. SgFunctionDeclaration* tmp =
  // const_cast<SgFunctionDeclaration *> (full_decl);
  // proto->set_definingDeclaration(tmp);
  // ROSE_ASSERT(proto->get_definingDeclaration() != NULL);
  // ROSE_ASSERT(proto->get_definingDeclaration() == NULL);

  // printf ("In generateFriendPrototype(): proto->get_definingDeclaration() =
  // %p \n",proto->get_definingDeclaration()); printf ("In
  // generateFriendPrototype(): full_decl = %p returning SgFunctionDeclaration
  // prototype = %p \n",full_decl,proto);

  // Make sure that internal referneces are to the same file (else the symbol
  // table information will not be consistant).
  ROSE_ASSERT(proto != NULL);
  ROSE_ASSERT(proto->get_firstNondefiningDeclaration() != NULL);

  // Note that the function prototype has not been inserted into the AST, so it
  // does not have a path to SgSourceFile.
  // ROSE_ASSERT(SageInterface::getEnclosingSourceFile(proto) == NULL);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(proto) != NULL);
  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(scope) != NULL);

  ROSE_ASSERT(SageInterface::getEnclosingSourceFile(
                  proto->get_firstNondefiningDeclaration()) != NULL);
  // printf
  // ("SageInterface::getEnclosingSourceFile(proto->get_firstNondefiningDeclaration())->getFileName()
  // = %s
  // \n",SageInterface::getEnclosingSourceFile(proto->get_firstNondefiningDeclaration())->getFileName().c_str());

  return proto;
}

//! Generates a 'friend' declaration from a given function declaration.
// For a friend declaration, two scopes are involved.
//'scope' is the class definition within which the friend declaration is
// inserted. 'class_scope' is the class definition's SgClassDeclaration's scope
// in which the function symbol should be created, if not exist.
static SgFunctionDeclaration *
generateFriendPrototype(const SgFunctionDeclaration *full_decl,
                        SgScopeStatement *scope,
                        SgScopeStatement *class_scope) {
  ROSE_ASSERT(class_scope != NULL);

  if (enable_debug) {
#ifdef __linux__
    cout << "Entering " << __PRETTY_FUNCTION__ << endl;
    cout << "\t source func decl is:" << full_decl << endl;
    full_decl->get_file_info()->display();
#endif

    cout << "\t target class definition is:" << scope << endl;
    scope->get_file_info()->display();

    cout << "\t target class declaration's scope is:" << class_scope << endl;
    class_scope->get_file_info()->display();
  }

  SgFunctionDeclaration *proto = generatePrototype(full_decl, scope, true);
  ROSE_ASSERT(proto != NULL);

  // Remove any 'extern' modifiers
  proto->get_declarationModifier().get_storageModifier().reset();

  // Set the 'friend' modifier
  proto->get_declarationModifier().setFriend();

  // DQ (2/26/2009): Remove the SgFunctionSymbol since this is a "friend"
  // function. Since this is a friend we don't want to have the SgFunctionSymbol
  // in "scope" so remove the symbol.  The SageBuilder function generated the
  // SgFunctionSymbol and at this point we need to remove it.  Friend function
  // don't have symbols in the class scope where they may appear as a
  // declaration.  The SageBuilder function could be provided a parameter to
  // indicate that a friend function is required, this would then suppress the
  // construction of the symbol in the scope's symbol table (and the scope of
  // the function is not the same as the class scope in this case as well.
  SgFunctionSymbol *friendFunctionSymbol =
      isSgFunctionSymbol(scope->lookup_symbol(full_decl->get_name()));
  ROSE_ASSERT(friendFunctionSymbol != NULL);
  // printf ("@@@@@@@@@@@@ In generateFriendPrototype(): removing
  // SgFunctionSymbol = %p with friendFunctionSymbol->get_declaration() = %p
  // \n",friendFunctionSymbol,friendFunctionSymbol->get_declaration());
  scope->remove_symbol(friendFunctionSymbol);

  {
    delete friendFunctionSymbol;
    friendFunctionSymbol = NULL;
  }

  // printf ("In generatePrototype(): Returning SgFunctionDeclaration prototype
  // = %p \n",proto);

  // ROSE_ASSERT(copyDeclarationStatement->get_firstNondefiningDeclaration()->get_definingDeclaration()
  // != NULL);

  ROSE_ASSERT(proto->get_definingDeclaration() == NULL);
  // proto->set_definingDeclaration(full_decl);

  return proto;
}

/*!
 *  \brief Beginning at the given declaration statement, this routine
 *  searches for first declaration in global scope that appears before
 *  this one.
 */
static SgDeclarationStatement *
findClosestGlobalInsertPoint(SgDeclarationStatement *f) {
  ROSE_ASSERT(f);
  SgDeclarationStatement *closest = f;
  SgNode *cur_parent = f->get_parent();
  while (cur_parent && !isSgGlobal(cur_parent)) {
    if (isSgDeclarationStatement(cur_parent))
      closest = isSgDeclarationStatement(cur_parent);
    cur_parent = cur_parent->get_parent();
  }
  return isSgGlobal(cur_parent) ? closest : 0;
}

/*!
 *  Traversal to insert a new global prototype.
 *
 *  This traversal searches for the first non-defining declaration for
 *  a given function definition, and inserts a new prototype of that
 *  function into global scope. In addition, it fixes up the
 *  definition's first non-defining declaration field to point to the
 *  new global prototype.
 *
 *  The traversal terminates as soon as the first matching declaration
 *  is found by throwing an exception as a string encoded with the
 *  word "done". The caller must ensure that any other matching
 *  declarations have their first non-defining declaration fields
 *  fixed up as well.
 */
class GlobalProtoInserter : public AstSimpleProcessing {
public:
  GlobalProtoInserter(SgFunctionDeclaration *def, SgGlobal *scope)
      : def_decl_(def), glob_scope_(scope), proto_(0) {}

  virtual void visit(SgNode *cur_node) {
    SgFunctionDeclaration *cur_decl = isSgFunctionDeclaration(cur_node);
    if (cur_decl && cur_decl->get_definingDeclaration() == def_decl_ &&
        !isSgGlobal(cur_decl->get_parent()))
    //        && isSgGlobal (cur_decl->get_parent ()) != glob_scope_)
    {
      proto_ = insertManually(def_decl_, glob_scope_, cur_decl);
      throw string("done");
    }
  }

  SgFunctionDeclaration *getProto(void) { return proto_; }
  const SgFunctionDeclaration *getProto(void) const { return proto_; }

  static SgFunctionDeclaration *

  insertManually(SgFunctionDeclaration *def_decl, SgGlobal *scope,
                 SgDeclarationStatement *target) {
    SgFunctionDeclaration *proto = generatePrototype(def_decl, scope, false);
    ROSE_ASSERT(proto);

    SgDeclarationStatement *insert_point = findClosestGlobalInsertPoint(target);
    ROSE_ASSERT(insert_point);

    ASTtools::moveBeforePreprocInfo(insert_point, proto);
    // ROSE_ASSERT(insert_point->get_scope() == scope);
    ROSE_ASSERT(find(scope->getDeclarationList().begin(),
                     scope->getDeclarationList().end(),
                     insert_point) != scope->getDeclarationList().end());

    scope->insert_statement(insert_point, proto, true);
    proto->set_parent(scope);
    proto->set_scope(scope);

    if (!Outliner::useNewFile) {
      // Liao, 12/20/2012. A hidden first non-defining declaration is built when
      // the defining one is created So the newly generated prototype function
      // declaration is no longer the first non-defining declaration.
      SgFunctionDeclaration *first_non_def =
          isSgFunctionDeclaration(proto->get_firstNondefiningDeclaration());
      ROSE_ASSERT(first_non_def != NULL);
      ROSE_ASSERT(first_non_def->get_symbol_from_symbol_table() != NULL);
      def_decl->set_firstNondefiningDeclaration(first_non_def);
      // Liao, we have to set it here otherwise it is difficult to find this
      // prototype later. Using static to avoid name collision
      SageInterface::setStatic(proto);
      SageInterface::setStatic(def_decl);
      SageInterface::setStatic(first_non_def);
    }

    return proto;
  }

private:
  //! Defining declaration.
  SgFunctionDeclaration *def_decl_;

  //! Global scope.
  SgGlobal *glob_scope_;

  //! New global prototype (i.e., new first non-defining declaration).
  SgFunctionDeclaration *proto_;
};

//! Inserts a prototype into the original global scope of the outline target
static SgFunctionDeclaration *insertGlobalPrototype(
    SgFunctionDeclaration *def, FuncDeclList_t &friendFunctionPrototypeList,
    SgGlobal *scope,
    SgDeclarationStatement
        *default_target) // The enclosing function for the outlining target
{
  SgFunctionDeclaration *prototype = NULL;

  if (def && scope) {
    // DQ (3/3/2009): Why does this code use try .. catch blocks (exception
    // handling)?
    GlobalProtoInserter ins(def, scope);
    try {
      ins.traverse(scope, preorder);
    } catch (string &s) {
      ROSE_ASSERT(s == "done");
    }
    prototype = ins.getProto();

    if (!prototype && default_target) // No declaration found
    {
      // Liao, 5/19/2009
      // The prototype has to be inserted to the very first class having a
      // friend declaration to the outlined function to avoid conflicting type
      // info. for extern "C" functions. The reason is that there is no way to
      // use friend and extern "C" together within a class.
      if (friendFunctionPrototypeList.size() != 0) {
        vector<SgDeclarationStatement *> origFriends;
        for (FuncDeclList_t::iterator i = friendFunctionPrototypeList.begin();
             i != friendFunctionPrototypeList.end(); i++) {
          SgDeclarationStatement *decl = isSgDeclarationStatement(*i);
          ROSE_ASSERT(decl != NULL);
          origFriends.push_back(decl);
        }
        vector<SgDeclarationStatement *> sortedFriends =
            SageInterface::sortSgNodeListBasedOnAppearanceOrderInSource(
                origFriends);
        prototype =
            GlobalProtoInserter::insertManually(def, scope, sortedFriends[0]);
      } else
        prototype =
            GlobalProtoInserter::insertManually(def, scope, default_target);
    }
  }

  // The friend function declarations are linked to the global declarations via
  // first non-defining declaration links. Fix-up remaining prototypes.
  if (prototype != NULL) {
    // printf ("In insertGlobalPrototype(): proto = %p protos.size() = %"
    // PRIuPTR " \n",prototype,friendFunctionPrototypeList.size());
    for (FuncDeclList_t::iterator i = friendFunctionPrototypeList.begin();
         i != friendFunctionPrototypeList.end(); ++i) {
      SgFunctionDeclaration *proto_i = *i;
      ROSE_ASSERT(proto_i);
      proto_i->set_firstNondefiningDeclaration(
          prototype->get_firstNondefiningDeclaration());
      ROSE_ASSERT(proto_i->get_declaration_associated_with_symbol() != NULL);

      // Only set the friend function prototype to reference the defining
      // declaration of we will NOT be moving the defining declaration to a
      // separate file.
      if (Outliner::useNewFile == false)
        proto_i->set_definingDeclaration(def);
    }

    // DQ (2/20/2009): Set the non-defining declaration
    if (def->get_firstNondefiningDeclaration() == NULL) {
      prototype->set_firstNondefiningDeclaration(prototype);
    }

    // DQ (2/20/2009): Added assertions.
    ROSE_ASSERT(prototype->get_parent() != NULL);
    ROSE_ASSERT(prototype->get_firstNondefiningDeclaration() != NULL);

    ROSE_ASSERT(prototype->get_definingDeclaration() != NULL);
  }

  // printf ("In insertGlobalPrototype(): Returning global SgFunctionDeclaration
  // prototype = %p \n",prototype);

  return prototype;
}

// DQ (1/17/2020): Adding support for outlining functions in a new file where
// the original file has a defining class declaration. This is an issue because
// the new file used to build the file where we place the outlined functions is
// a copy of the original file and so it will contain a second copy of the
// defining class declaration.  This second copy of the defining class
// declaration is not a violation of ODR if it has the same token sequence
// (which should include the friend declaration), however more critically, the
// outline function will not compile if it has a reference to a private or
// protected member variable or function and so the friend function must be also
// inserted into the second defining class declaration.

class FindMatchingDefiningClassDeclarationTraversal
    : public SgSimpleProcessing {
  // DQ (1/16/2020): File the matching defining declaration in another file.
  // Adding support for friend function declarations to b added to matching
  // class declarations in other files.
public:
  SgClassDeclaration *pattern;
  std::string pattern_class_name;
  SgClassDeclaration *matchingClassDeclaration;
  // target is actually the source pattern we want to find a match for it.
  FindMatchingDefiningClassDeclarationTraversal(SgClassDeclaration *target);

  void visit(SgNode *astNode);
};

FindMatchingDefiningClassDeclarationTraversal::
    FindMatchingDefiningClassDeclarationTraversal(SgClassDeclaration *target) {
  ROSE_ASSERT(target != NULL);
  pattern = target;
  pattern_class_name = pattern->get_mangled_name();
  matchingClassDeclaration = NULL;
}
//! Check if a node is a defining class declaration and its mangled name match a
//! given pattern class name.
void FindMatchingDefiningClassDeclarationTraversal::visit(SgNode *astNode) {
  SgClassDeclaration *target = isSgClassDeclaration(astNode);

  ROSE_ASSERT(pattern != NULL);

  // Looking only for defining declarations.
  if (target != NULL && target == target->get_definingDeclaration()) {
    string target_class_name = target->get_mangled_name();
    if (target_class_name == pattern_class_name) {
      if (enable_debug) {
        printf("Found a matching name! target_class_name = %s \n",
               target_class_name.c_str());
        target->get_file_info()->display();
      }

      matchingClassDeclaration = target;
    }
  }
}

SgClassDeclaration *
findMatchingDefiningClassDeclaration(SgClassDeclaration *target);

SgClassDeclaration *
findMatchingDefiningClassDeclaration(SgSourceFile *targetFile,
                                     SgClassDeclaration *target) {
  // DQ (1/16/2020): Adding support for when the matching class is in the "*.C"
  // source file, and thus appears in the generated "_lib.C" file as well. Since
  // we no long share class declarations, we need to insert the friend function
  // into all possible defining class declarations across all files.
  if (enable_debug) {
#ifdef __linux__
    cout << "Entering " << __PRETTY_FUNCTION__
         << " for the following pattern class declaration: " << endl;
#endif
    target->get_file_info()->display();
  }

  FindMatchingDefiningClassDeclarationTraversal t(target);

  t.traverseWithinFile(targetFile, preorder);

  return t.matchingClassDeclaration;
}

/*!
 *  \brief Given a 'friend' declaration, insert it into the given
 *  class definition.
 */
static SgFunctionDeclaration *
insertFriendDecl(const SgFunctionDeclaration *func,
                 SgGlobal *scope,            // the relevant class's scope
                 SgClassDefinition *cls_def) // the class definition in which we
                                             // insert friend declarations
{
  SgFunctionDeclaration *friend_proto = 0;

  if (enable_debug)
    printf("Entering insertFriendDecl(): func = %p \n", func);

  if (func && scope && cls_def) {
    // Determine insertion point, i.
    SgDeclarationStatementPtrList &mems = cls_def->get_members();
    SgDeclarationStatementPtrList::iterator i = mems.begin();

    // Create the friend declaration.  cls_def: the class definition ,  scope:
    // the corresponding class's scope
    friend_proto = generateFriendPrototype(func, cls_def, scope);
    ROSE_ASSERT(friend_proto != NULL);
    ROSE_ASSERT(friend_proto->get_definingDeclaration() == NULL);

    if (enable_debug) {
      printf("In insertFriendDecl(): Built SgFunctionDeclaration: friend_proto "
             "= %p = %s name = %s \n",
             friend_proto, friend_proto->class_name().c_str(),
             friend_proto->get_name().str());
      {
        bool isExtern = friend_proto->get_declarationModifier()
                            .get_storageModifier()
                            .isExtern();
        bool linkageSpecified = (friend_proto->get_linkage().empty() == false);
        bool isFriend = friend_proto->get_declarationModifier().isFriend();

        printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s \n",
               isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
               isFriend ? "true" : "false");
      }
    }

    // Insert it into the class.
    if (i != mems.end()) {
    }

    // DQ (8/6/2019): This is the step we want to defer to later so that we can
    // support an optimization of where this is done within header files.
    // cls_def->get_members().insert(i, friend_proto);
    bool includingSelf = true;
    SgSourceFile *sourceFile =
        SageInterface::getEnclosingNode<SgSourceFile>(cls_def, includingSelf);
    ROSE_ASSERT(sourceFile != NULL);

    if (enable_debug) {
      printf(
          "In insertFriendDecl(): sourceFile->get_unparseHeaderFiles() = %s \n",
          sourceFile->get_unparseHeaderFiles() ? "true" : "false");
      printf(
          "In insertFriendDecl(): Outliner::useNewFile                 = %s \n",
          Outliner::useNewFile ? "true" : "false");
    }

    // DQ (1/17/2020): This should be a predict specific to if we are outlineing
    // code to a seperate file. Also, what we do here might depende more of if
    // the class definition is in the source file or not. if
    // (sourceFile->get_unparseHeaderFiles() == true)
    if (Outliner::useNewFile == true) {
      // DQ (8/6/2019): This is the new behavior designed to optimize the header
      // file unparsing. Specifically we want to only unparse header files that
      // contain transformations, this is because the overhead of processing
      // header files can be a bit high incuring this for every header files
      // (then can be thousands) is an unnecessary cost because the typical use
      // case is that only one header file need be unparsed for each source file
      // that is processed. This significaly optimizes the performance of tools
      // that are using the outliner.

      // DQ (8/7/2019): Instead, save the information to use later to support
      // the transformation, *i and friend_proto.
      // cls_def->get_members().insert(i, friend_proto);
      //   bool inFront = true;
      // SgStatement::insert_statement(*i,friend_proto,inFront);
      // cls_def->get_members().insert(i, friend_proto);

      ROSE_ASSERT(*i != NULL);
      ROSE_ASSERT(i != mems.end());

      if (enable_debug) {
        // DQ (10/8/2019): Output when function declarations are being inserted.
        printf("#################################################### \n");
        printf("Inserting friend_proto = %p into cls_def = %p (this should "
               "have been defered) \n",
               friend_proto, cls_def);
        printf("#################################################### \n");
      }
      // DQ (8/7/2019): This is the only form of insert that appears to work
      // well.
      cls_def->get_members().insert(i, friend_proto);

      // DQ (1/17/2020): Now we need to see the class definition is from the
      // input file (*.C) file and if so there will be another class definition
      // in the generated file for the outline functions (*_lib.C file). And we
      // need to add a copy of the friend function there as well.  This is
      // important for ODR generally, but more specifically becasue the outlined
      // function accessing the member variable has will be in the generated
      // file with the second class definition.

      string filename =
          cls_def->get_startOfConstruct()->get_physical_filename();
      // SgSourceFile* inputFile =
      // SageInterface::getEnclosingSourceFile(cls_def); ROSE_ASSERT(inputFile
      // != NULL);
      bool includingSelf = false;
      SgClassDeclaration *targetClassDeclaration = cls_def->get_declaration();
      ROSE_ASSERT(targetClassDeclaration != NULL);

      SgSourceFile *sourceFileOfClassDeclaration =
          getEnclosingSourceFile(targetClassDeclaration, includingSelf);
      ROSE_ASSERT(sourceFileOfClassDeclaration != NULL);
      bool filename_matches_source_file =
          (filename == sourceFileOfClassDeclaration->getFileName());
      if (filename_matches_source_file == true) {
        if (enable_debug)
          printf("Look for other matching class definitions in the associated "
                 "file where the outlined functions are put \n");

        SgProject *project = getEnclosingNode<SgProject>(
            sourceFileOfClassDeclaration, includingSelf);

        // DQ (1/16/2020): Check for the possability of a second class if this
        // class was in the source file that was copied.
        SgFileList *fileListNode = project->get_fileList_ptr();
        SgFilePtrList &fileList = fileListNode->get_listOfFiles();
        if (enable_debug)
          printf("#################### fileList.size() = %zu \n",
                 fileList.size());

        for (size_t i = 0; i < fileList.size(); i++) {
          SgSourceFile *alternativeSourceFile = isSgSourceFile(fileList[i]);
          ROSE_ASSERT(alternativeSourceFile != NULL);
          if (enable_debug)
            printf("alternativeSourceFile = %p = %s \n", alternativeSourceFile,
                   alternativeSourceFile->getFileName().c_str());

          if (alternativeSourceFile != sourceFileOfClassDeclaration) {
            // Search for defining class declaration matching
            // sourceFileOfClassDeclaration.
            if (enable_debug)
              printf("Search for class name = %s in file = %s \n",
                     targetClassDeclaration->get_name().str(),
                     alternativeSourceFile->getFileName().c_str());

            // Search in file for additional defining declaration matching
            // targetClassDeclaration.
            SgClassDeclaration *matchingClassDeclaration =
                findMatchingDefiningClassDeclaration(alternativeSourceFile,
                                                     targetClassDeclaration);
            if (matchingClassDeclaration != NULL) {

              SgClassDefinition *matchingClassDefinition =
                  matchingClassDeclaration->get_definition();
              ROSE_ASSERT(matchingClassDefinition != NULL);

              // insert a copy of the friend function declaration into the
              // matchingClassDeclaration.
              size_t orig_count = matchingClassDefinition->get_members().size();
              if (enable_debug) {
                printf(" --- insert a copy of the friend function declaration "
                       "into the matchingClassDeclaration's definition: "
                       "members size=%lu\n",
                       matchingClassDefinition->get_members().size());
                matchingClassDefinition->get_file_info()->display();
              }

              SgDeclarationStatementPtrList::iterator i2 =
                  matchingClassDefinition->get_members().begin();
              ROSE_ASSERT(i2 != matchingClassDefinition->get_members().end());

              // Initially we can test this by making a copy of the pointer, but
              // later it should be deep copy.
              // SgFunctionDeclaration* friendFunction = friend_proto;
              SgFunctionDeclaration *friendFunction = generateFriendPrototype(
                  func, matchingClassDefinition,
                  matchingClassDeclaration->get_scope());
              ;

              matchingClassDefinition->get_members().insert(i2, friendFunction);

              // Also mark the class definition as transformed.
              matchingClassDefinition->set_isModified(true);
              ROSE_ASSERT(orig_count + 1 ==
                          matchingClassDefinition->get_members().size());
              friendFunction->set_parent(matchingClassDefinition);
              friendFunction->set_scope(matchingClassDeclaration->get_scope());
              friendFunction->get_startOfConstruct()->set_physical_filename(
                  alternativeSourceFile->getFileName().c_str());
              friendFunction->get_endOfConstruct()->set_physical_filename(
                  alternativeSourceFile->getFileName().c_str());
              friendFunction->set_isModified(true);
              if (enable_debug) {
                cout << "after insertion, checking the matching class "
                        "definition for the result,  members size="
                     << matchingClassDefinition->get_members().size() << endl;
                matchingClassDeclaration->unparseToString();
              }
            }
          }
        } // end for (fileLIst)

      } else {
        if (enable_debug)
          cout << "The class definition is in a header file (not the input "
                  "file), so we should not have to worry about any repeated "
                  "definition."
               << endl;
      }

    } else {
      // If we are not unparsing headers we still have to worry about class
      // declarations that are in the original source file (*.C input file) and
      // which would be duplicated in the copy of the original source file when
      // we outline to a seperate file.

      if (enable_debug) {
        // DQ (10/8/2019): Output when function declarations are being inserted.
        printf("#################################################### \n");
        printf("Inserting friend_proto = %p into cls_def = %p (this should "
               "have been defered) \n",
               friend_proto, cls_def);
        printf("#################################################### \n");
      }

      // DQ (8/6/2019): This is the normal (original) behavior.
      cls_def->get_members().insert(i, friend_proto);
    }

    friend_proto->set_parent(cls_def);
    friend_proto->set_scope(scope);

    // Address the iterator invalidation caused by the insertion of a new
    // element into the list. i = mems.begin(); ROSE_ASSERT(*i == friend_proto);
    // i++;

    // DQ (6/4/2019): The isModified flag is reset by the SageBuilder functions
    // as part of adding the function. It is however marked as a transformation.
    // To support the header file unparsing, we also need to set the physical
    // file name to the header file name.
    string filename = cls_def->get_startOfConstruct()->get_physical_filename();

    friend_proto->get_startOfConstruct()->set_physical_filename(filename);
    friend_proto->get_endOfConstruct()->set_physical_filename(filename);

    // DQ (6/4/2019): Need to mark this as a modification, so that it will be
    // detected as something to trigger the output of the header file when the
    // class declaration appears in a header file. Maybe the insert function
    // should do this?
    friend_proto->set_isModified(true);
    cls_def->set_isModified(true);
  }

  //  printf ("In insertFriendDecl(): Returning SgFunctionDeclaration prototype
  //  = %p \n",friend_proto); We should not try to unparse the friend
  //  declaration here. Since its first-non definining declaration has not yet
  //  been inserted. So it has no declaration associated with a symbol
  //  cout<<friend_proto->unparseToString()<<endl;

  if (enable_debug) {
    ROSE_ASSERT(friend_proto != NULL);
    printf("Exiting insertFriendDecl(): func = %p friend_proto = %p "
           "friend_proto->isFriend = %s \n",
           func, friend_proto,
           friend_proto->get_declarationModifier().isFriend() ? "true"
                                                              : "false");
  }

  return friend_proto;
}

/*!
 *  \brief Returns 'true' if the given declaration statement is marked
 *  as 'private' or 'protected'.
 */
static bool isProtPriv(const SgDeclarationStatement *decl) {

  if (decl) {
    SgDeclarationStatement *decl_tmp =
        const_cast<SgDeclarationStatement *>(decl);

    // Liao 3/1/2013. workaround a bug introduced by Dan: only the defining decl
    // has the correct access modifier.
    if (decl_tmp->get_definingDeclaration() != NULL) {
      decl_tmp = decl_tmp->get_definingDeclaration();
    }

    ROSE_ASSERT(decl_tmp);
    const SgAccessModifier &decl_access_mod =
        decl_tmp->get_declarationModifier().get_accessModifier();
    if (decl_access_mod.isPrivate() || decl_access_mod.isProtected())
      return true;
    if (decl_access_mod.isDefault()) {
      const SgScopeStatement *decl_scope = decl_tmp->get_scope();
      const SgClassDefinition *class_def = isSgClassDefinition(decl_scope);
      if (class_def != NULL && class_def->get_declaration()->get_class_type() ==
                                   SgClassDeclaration::e_class) {
        return true;
      }
    }
  }

  return false;
}

static bool classHasNonPublicConstructor(SgClassDefinition *cl_def) {
  if (cl_def == NULL)
    return false;
  for (SgDeclarationStatement *member : cl_def->get_members()) {
    SgMemberFunctionDeclaration *func_decl =
        isSgMemberFunctionDeclaration(member);
    if (func_decl == NULL)
      continue;
    if (!func_decl->get_specialFunctionModifier().isConstructor())
      continue;
    if (isProtPriv(func_decl))
      return true;
  }
  return false;
}

/*!
 *  \brief Returns 'true' if the given variable use is a 'protected'
 *  or 'private' class member.
 */
static SgClassDefinition *isProtPrivMember(SgVarRefExp *v) {
  if (v) {
    SgVariableSymbol *sym = v->get_symbol();
    if (sym) {
      SgInitializedName *name = sym->get_declaration();
      ROSE_ASSERT(name != NULL);
      SgClassDefinition *cl_def = isSgClassDefinition(name->get_scope());
      if (cl_def != NULL) {
      }

      if (cl_def && isProtPriv(name->get_declaration())) {
        return cl_def;
      } else {
      }
    }
  }

  return NULL; // default: is not
}

/*!
 *  \brief Returns 'true' if the given type was declared as a
 *  'protected' or 'private' class member.
 *  This function need to be recursive. It may involves a chain of base types
 * and some types in the middle are private class types. The deepest base type
 * may not be a private type.
 */
static SgClassDefinition *isProtPrivType(SgType *t) {
  if (t) {
    // check self
    if (t && isSgNamedType(t)) {
      SgNamedType *named = isSgNamedType(t);
      ROSE_ASSERT(named);
      SgDeclarationStatement *decl = named->get_declaration();
      if (decl)
        if (decl->get_definingDeclaration())
          decl = decl->get_definingDeclaration();
      if (isProtPriv(decl)) {
        // if any type in the type chain is private, we return immediately.
        if (SgClassDefinition *c_d = isSgClassDefinition(decl->get_parent()))
          return c_d;
        // if c_d is NULL, we go further to check its base type's access
        // control;
      }
    }

    // recursively check base type if any
    if (SgModifierType *m_type = isSgModifierType(t))
      return isProtPrivType(m_type->get_base_type());
    else if (SgPointerType *p_type = isSgPointerType(t))
      return isProtPrivType(p_type->get_base_type());
    else if (SgArrayType *a_type = isSgArrayType(t))
      return isProtPrivType(a_type->get_base_type());
    else if (SgReferenceType *r_type = isSgReferenceType(t))
      return isProtPrivType(r_type->get_base_type());
    else if (SgTypedefType *t_type = isSgTypedefType(t))
      return isProtPrivType(t_type->get_base_type());
  }

  // DQ (11/3/2015): Fixed compiler warning.
  // return false;
  return NULL;
}

/*!
 *  \brief Returns 'true' if the given member function is 'protected'
 *  or 'private'.
 */
static SgClassDefinition *isProtPrivMember(SgMemberFunctionRefExp *f) {
  if (f) {
    SgMemberFunctionSymbol *sym = f->get_symbol();
    if (sym) {
      SgMemberFunctionDeclaration *f_decl = sym->get_declaration();
      ROSE_ASSERT(f_decl);
      SgClassDefinition *cl_def = sym->get_scope();
      if (cl_def && isProtPriv(f_decl))
        return cl_def;
    }
  }
  return NULL; // default: is not
}

/*!
 *  \brief Inserts all necessary friend declarations.
 *
 *  \returns A list, 'friends', of all generated friend declarations.
 *  func: the generated outlined function
 */
// static void insertFriendDecls (SgFunctionDeclaration* func, SgGlobal* scope,
// FuncDeclList_t& friends)
static
    // DQ (11/19/2020): DeferredTransformation support was moved to the
    // SageInterface namespace to support more general usage.
    // Outliner::DeferredTransformation
    SageInterface::DeferredTransformation
    insertFriendDecls(SgFunctionDeclaration *func, SgGlobal *scope,
                      FuncDeclList_t &friends) {
  if (enable_debug) {
    printf("************************************************************ \n");
    printf("TOP of insertFriendDecls(): func = %p = %s \n", func,
           func->class_name().c_str());
    bool isExtern =
        (func->get_declarationModifier().get_storageModifier().isExtern() ==
         true);
    bool linkageSpecified = (func->get_linkage().empty() == false);
    bool isFriend = (func->get_declarationModifier().isFriend() == true);
    bool isDefiningDeclaration = (func->get_definition() != NULL);
    printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s "
           "isDefiningDeclaration = %s \n",
           isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
           isFriend ? "true" : "false",
           isDefiningDeclaration ? "true" : "false");

    // printf ("In insertFriendDecls(): func = %p = %s name = %s
    // \n",func,func->class_name().c_str(),func->get_name().str());
    printf(" --- scope = %p = %s \n", scope, scope->class_name().c_str());
    printf(" --- friends list size = %" PRIuPTR " \n", friends.size());
    printf("************************************************************ \n");
  }

  // DQ (11/19/2020): DeferredTransformation support was moved to the
  // SageInterface namespace to support more general usage. DQ (8/13/2019):
  // Adding return value, used when header file unparsing is active.
  // Outliner::DeferredTransformation deferedFriendTransformation;
  SageInterface::DeferredTransformation deferedFriendTransformation;

  if (func && scope) {
    if (enable_debug)
      printf("In insertFriendDecls(): friends list size = %" PRIuPTR " \n",
             friends.size());

    // Collect a list of all classes that need a 'friend' decl.
    // The outlining target has accesses to those classes' private/protected
    // members
    typedef set<SgClassDefinition *> ClassDefSet_t;
    ClassDefSet_t classes;

    // better algorithm in one pass to find all types of nodes
    RoseAst ast(func);
    for (RoseAst::iterator i = ast.begin(); i != ast.end(); ++i) {
      SgClassDefinition *cl_def = NULL;
      // variable declarations may use some private types.
      if (SgInitializedName *init_name = isSgInitializedName(*i))
        cl_def = isProtPrivType(init_name->get_type());
      else if (SgVarRefExp *v_ref = isSgVarRefExp(*i)) {
        cl_def = isProtPrivMember(v_ref);

        if (enable_debug) {
          printf("In insertFriendDecls(): after isProtPrivMember(): cl_def = "
                 "%p \n",
                 cl_def);
          SgVariableSymbol *variableSymbol = v_ref->get_symbol();
          ROSE_ASSERT(variableSymbol != NULL);
          SgInitializedName *initializedName =
              variableSymbol->get_declaration();
          ROSE_ASSERT(initializedName != NULL);
          printf("In insertFriendDecls(): v_ref = %p = %s initializedName name "
                 "= %s \n",
                 v_ref, v_ref->class_name().c_str(),
                 initializedName->get_name().str());
          printf("In insertFriendDecls(): cl_def = %p \n", cl_def);
        }
        if (cl_def == NULL) {
          if (enable_debug) {
            ROSE_ASSERT(v_ref->get_type() != NULL);
            printf("Calling isProtPrivType(): v_ref->get_type() = %p = %s \n",
                   v_ref->get_type(), v_ref->get_type()->class_name().c_str());
          }
          cl_def = isProtPrivType(v_ref->get_type());
        }
      } else if (SgEnumVal *v_ref = isSgEnumVal(*i)) {
        // EnumVal may have a private type: TODO: all expressions should be
        // check against private type
        if (enable_debug) {
          ROSE_ASSERT(v_ref->get_type() != NULL);
          printf("Calling isProtPrivType(): v_ref->get_type() = %p = %s \n",
                 v_ref->get_type(), v_ref->get_type()->class_name().c_str());
        }
        cl_def = isProtPrivType(v_ref->get_type());
      } else if (SgMemberFunctionRefExp *f_ref = isSgMemberFunctionRefExp(*i))
        cl_def = isProtPrivMember(f_ref);
      else if (SgConstructorInitializer *ctor =
                   isSgConstructorInitializer(*i)) {
        // C++ constructors are called through SgConstructorInitializer, not
        // SgMemberFunctionRefExp.
        SgMemberFunctionDeclaration *fuc_decl = ctor->get_declaration();
        SgClassDeclaration *cls_decl = ctor->get_class_decl();
        if (!cls_decl) {
          printf("Warning: calling classes.insert() friend decl: constructor "
                 "has no class declaration = %p = %s \n",
                 ctor, ctor->class_name().c_str());
          continue;
        } else {
          SgClassDeclaration *def_cls_decl =
              isSgClassDeclaration(cls_decl->get_definingDeclaration());
          if (!def_cls_decl) {
            printf("Calling classes.insert(): constructor's class declaration "
                   "has no defining declaration = %p = %s \n",
                   ctor, ctor->class_name().c_str());
            continue;
          }

          SgClassDefinition *class_def = def_cls_decl->get_definition();
          if (fuc_decl != NULL) {
            if (isProtPriv(fuc_decl))
              cl_def = class_def;
          } else if (classHasNonPublicConstructor(class_def)) {
            cl_def = class_def;
          }
        }
      }

      if (cl_def != NULL) {
        if (enable_debug)
          printf("Calling classes.insert(): variables: cl_def = %p = %s \n",
                 cl_def, cl_def->class_name().c_str());
        classes.insert(cl_def);
      }
    } // end for RoseAst iterator

    // DQ (8/13/2019): Set the target classes.
    if (enable_debug)
      printf("Set the targetClasses: (disabled): "
             "deferedFriendTransformation.targetClasses = classes \n");
    deferedFriendTransformation.targetClasses = classes;

    // Insert 'em
    for (ClassDefSet_t::iterator c = classes.begin(); c != classes.end(); ++c) {
      ROSE_ASSERT(*c);
      if (enable_debug) {
        SgClassDefinition *classDefinition = *c;
        SgClassDeclaration *classDeclaration =
            classDefinition->get_declaration();
        if (enable_debug)
          printf("Building friend function for classDeclaration = %p = %s name "
                 "= %s \n",
                 classDeclaration, classDeclaration->class_name().c_str(),
                 classDeclaration->get_name().str());
      }
      // scope: the global scope in which the symbols should be inserted
      // class definition: the scope in which we insert friend declarations
      SgFunctionDeclaration *friend_decl = insertFriendDecl(func, scope, *c);
      ROSE_ASSERT(friend_decl != NULL);
      if (enable_debug) {
        printf("+++++++++++++++++++ friend_decl = %p = %s \n", friend_decl,
               friend_decl->class_name().c_str());
        bool isExtern = (friend_decl->get_declarationModifier()
                             .get_storageModifier()
                             .isExtern() == true);
        bool linkageSpecified = (friend_decl->get_linkage().empty() == false);
        bool isFriend =
            (friend_decl->get_declarationModifier().isFriend() == true);
        bool isDefiningDeclaration = (friend_decl->get_definition() != NULL);
        printf(" --- isExtern = %s linkageSpecified = %s isFriend = %s "
               "isDefiningDeclaration = %s \n",
               isExtern ? "true" : "false", linkageSpecified ? "true" : "false",
               isFriend ? "true" : "false",
               isDefiningDeclaration ? "true" : "false");
      }
      // DQ (2/23/2009): Added assertion.
      ROSE_ASSERT(friend_decl->get_definingDeclaration() == NULL);

      ROSE_ASSERT(friend_decl->get_scope() == scope);

      // DQ (2/27/2009): If we are outlining to a separate file, then we don't
      // want to attach a reference to the defining declaration which will be
      // moved to the different file (violates file consistency rules that are
      // not well enforced).
      ROSE_ASSERT(friend_decl->get_definingDeclaration() == NULL);
      if (enable_debug)
        printf("friend_decl = %p friend_decl->get_definingDeclaration() = %p "
               "friends list size = %zu \n",
               friend_decl, friend_decl->get_definingDeclaration(),
               friends.size());
      friends.push_back(friend_decl);
    }

    if (enable_debug)
      printf("friends list size = %zu \n", friends.size());

    // DQ (12/5/2019): This value can be greater than one, but it is not clear
    // what the reproducer is that will cause this. DQ (8/16/2019): After
    // discussion with Liao, assert that this is zero or one, since we can't see
    // how one outlined function could cause it to be a friend of two classes.
    // At the very least an example of this is not clear, and we want this
    // assertion to identify where this can happen.
    // ROSE_ASSERT(deferedFriendTransformation.targetClasses.size() < 2);
    if (deferedFriendTransformation.targetClasses.size() >= 2) {
      printf("NOTE: In insertFriendDecls(): "
             "deferedFriendTransformation.targetClasses.size() = %zu \n",
             deferedFriendTransformation.targetClasses.size());
    }
    // DQ (4/19/2022): An essential application code demonstrates that this
    // value can sometimes be as great as 4, so I have increased the limit. DQ
    // (12/11/2019): Modified to increase bound (required for tool_G using some
    // of the later gregression tests (after test_33.cpp).
    // ROSE_ASSERT(deferedFriendTransformation.targetClasses.size() <= 2);
    // ROSE_ASSERT(deferedFriendTransformation.targetClasses.size() <= 3);
    ROSE_ASSERT(deferedFriendTransformation.targetClasses.size() <= 4);
    // ROSE_ASSERT(deferedFriendTransformation.targetFriends.size() < 2);
    ROSE_ASSERT(deferedFriendTransformation.targetFriends.size() == 0);
  } else {
    // DQ (1/17/2020): Adding debugging information.
    if (func == NULL) {
      printf("NOTE: In insertFriendDecls(): func == NULL \n");
    } else {
      printf("NOTE: In insertFriendDecls(): scope == NULL: func = %p = %s \n",
             func, func->class_name().c_str());
    }

    if (scope == NULL) {
      printf("NOTE: In insertFriendDecls(): scope == NULL \n");
    } else {
      printf("NOTE: In insertFriendDecls(): func == NULL: scope = %p = %s \n",
             scope, scope->class_name().c_str());
    }
  }

  if (enable_debug) {
    printf("******************************************************** \n");
    printf("Leaving insertFriendDecls(): friends list size = %" PRIuPTR " \n",
           friends.size());
    printf("******************************************************** \n");
  }

  return deferedFriendTransformation;
}

// =====================================================================
//! Insert func into scope (could be either original scope or the new scope from
//! a new file),
//  and insert necessary declarations into the global scope of
//  target's original enclosing function).

// DQ (11/19/2020): DeferredTransformation support was moved to the
// SageInterface namespace to support more general usage. DQ (8/15/2019): Adding
// support to defer the transformations to header files. void Outliner::insert
// (SgFunctionDeclaration* func, SgGlobal* scope, SgBasicBlock*
// target_outlined_code ) Outliner::DeferredTransformation
SageInterface::DeferredTransformation
Outliner::insert(SgFunctionDeclaration *func, SgScopeStatement *scope,
                 SgBasicBlock *target_outlined_code) {
  // Scope is the global scope of the outlined location (could be in a separate
  // file).
  ROSE_ASSERT(func != NULL && scope != NULL);
  ROSE_ASSERT(Outliner::isValidOutliningScope(scope));
  ROSE_ASSERT(target_outlined_code != NULL);

  // DQ (9/26/2019): Trying to trace down where there is a
  // SgFunctionParameterList with parent not being set!
  ROSE_ASSERT(func->get_parameterList()->get_parent() != NULL);

  SgFunctionDeclaration *target_func = const_cast<SgFunctionDeclaration *>(
      SageInterface::getEnclosingFunctionDeclaration(target_outlined_code));
  ROSE_ASSERT(target_func != NULL);

  // DQ (9/26/2019): Trying to trace down where there is a
  // SgFunctionParameterList with parent not being set!
  ROSE_ASSERT(target_func->get_parameterList()->get_parent() != NULL);

  // This is the global scope of the original file
  SgGlobal *src_global = SageInterface::getGlobalScope(target_func);
  ROSE_ASSERT(src_global != NULL);

  // The scopes are the same only if this the outlining is NOT being output to a
  // separate file.
  if (Outliner::useNewFile) {
    ROSE_ASSERT(scope != src_global);
  } else {
    const bool scope_is_module =
        SageInterface::is_Fortran_language() &&
        Outliner::isFortranModuleDefinitionScope(scope);
    ROSE_ASSERT(scope == src_global || scope_is_module);
  }

  // Make sure this is a defining function
  ROSE_ASSERT(func->get_definition() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration() == func);

  SgSourceFile *enclosingSourceFile = getEnclosingSourceFile(scope, false);
  ROSE_ASSERT(enclosingSourceFile != NULL);

  int enclosingSourceFileId =
      enclosingSourceFile->get_file_info()->get_physical_file_id();
  int enclosingSourceFileIdFromMap =
      Sg_File_Info::get_nametofileid_map()[enclosingSourceFile->getFileName()];

  string filenameFromID =
      Sg_File_Info::getFilenameFromID(enclosingSourceFileId);

  ROSE_ASSERT(func->get_definingDeclaration()->get_file_info() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration()->get_startOfConstruct() != NULL);
  ROSE_ASSERT(func->get_definingDeclaration()->get_endOfConstruct() != NULL);

  // func->get_definingDeclaration()->get_file_info()->set_physical_file_id(enclosingSourceFileId);
  // func->get_definingDeclaration()->get_startOfConstruct()->set_physical_file_id(enclosingSourceFileId);
  // func->get_definingDeclaration()->get_endOfConstruct()
  // ->set_physical_file_id(enclosingSourceFileId);
  func->get_definingDeclaration()->get_startOfConstruct()->set_physical_file_id(
      enclosingSourceFileIdFromMap);
  func->get_definingDeclaration()->get_endOfConstruct()->set_physical_file_id(
      enclosingSourceFileIdFromMap);

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  // The defining function was generated by SageBuilder function
  // with the right target scope, so its symbol exists.
  // But we will insert prototypes for C/C++ ( not for Fortran) later and
  // the function symbol will be re-generated when the function prototypes are
  // generated So we need to remove the symbol for C/C++ and keep it for Fortran
  // Liao, 3/11/2009
  //   if (SageInterface::is_Fortran_language() != true)
  /*     {
         // The constructed defining declaration should have a symbol in its
     scope, remove it. SgFunctionSymbol* definingFunctionSymbol =
     isSgFunctionSymbol(scope->lookup_symbol(func->get_name()));
         ROSE_ASSERT(definingFunctionSymbol != NULL);
         scope->remove_symbol(definingFunctionSymbol);
         delete definingFunctionSymbol;
         definingFunctionSymbol = NULL;
         ROSE_ASSERT(scope->lookup_symbol(func->get_name()) == NULL);
       }
  */

  // Put the input function into the target scope
  SageInterface::appendStatement(func, scope);
  func->set_scope(scope);
  func->set_parent(scope);

  // DQ (11/9/2019): When used in conjunction with header file unparsing we need
  // to set the physical file id on entirety of the subtree being inserted.
  SageBuilder::fixupSourcePositionFileSpecification(func, filenameFromID);

  // DQ (11/19/2020): DeferredTransformation support was moved to the
  // SageInterface namespace to support more general usage. DQ (8/15/2019):
  // Adding support to defere the transformations in header files (a performance
  // improvement). DeferredTransformation headerFileTransformation;
  SageInterface::DeferredTransformation headerFileTransformation;

  // Error checking...
  if (Outliner::useNewFile == false) {
    ROSE_ASSERT(func->get_scope() == scope);
    if (!SageInterface::is_Fortran_language() || scope == src_global) {
      ROSE_ASSERT(func->get_scope() == src_global);
      ROSE_ASSERT(scope == src_global);
    } else {
      ROSE_ASSERT(Outliner::isFortranModuleDefinitionScope(scope));
    }
  } else {
    // DQ (6/15/2019): Adding more debugging information.
    if (scope == src_global) {
      printf("error: scope == src_global: scope = %p = %s \n", scope,
             scope->class_name().c_str());
      ROSE_ASSERT(scope->get_file_info() != NULL);
      scope->get_file_info()->display(
          "error: In Outliner::insert(): scope == src_global : debug");

      printf(" --- func = %p = %s name = %s \n", func,
             func->class_name().c_str(), func->get_name().str());
      ROSE_ASSERT(func->get_file_info() != NULL);
      func->get_file_info()->display(
          "error: In Outliner::insert(): func : debug");
    }
    // DQ (6/15/2019): This is the only assertion that might be best.
    ROSE_ASSERT(func->get_scope() == scope);
  }

  // no need to build nondefining function prototype for Fortran, Liao,
  // 3/11/2009 if (SageInterface::is_Fortran_language() == true) return;

  // I don't understand what this is (appears to be a list of outlined function
  // prototypes (non-defining declarations)). It is used by both the
  // insertGlobalPrototype() and
  FuncDeclList_t friendFunctionPrototypeList;

  if (SageInterface::is_Fortran_language() == false) {
    // Insert all necessary 'friend' declarations. This step will not build
    // symbols for the symbol table (although the build functions will they are
    // removed in the insertFriendDecls() function).

    // DQ (9/26/2019): I think that the friend function declaration being used
    // is the wrong one, and thus is is being used twice in the AST. The
    // initialization of the headerFileTransformation can only be handled
    // partially (filling in the class declaration/definition, but not the
    // function prototype). DQ (8/7/2019): Save the information to support the
    // header file (class definition) to be done later (and optimization for
    // header file unparsing). insertFriendDecls (func, src_global,
    // friendFunctionPrototypeList); Outliner::DeferredTransformation
    // headerFileTransformation = insertFriendDecls (func, src_global,
    // friendFunctionPrototypeList);
    headerFileTransformation =
        insertFriendDecls(func, src_global, friendFunctionPrototypeList);
  }
  SgFunctionDeclaration *sourceFileFunctionPrototype = NULL;

  // insert a pointer to function declaration if use_dlopen is true
  // insert it into the original global scope
  // No need to generate this declaration if we use the simple call convention.
  if (use_dlopen && !use_dlopen_simple) {
    // void (*OUT_xxx__p) (void**); // this parameter type depends on the number
    // of variables. If zero variables, empty parameter.
    SgFunctionParameterTypeList *tlist = buildFunctionParameterTypeList();
    // Only if func's argument list is not emtpy
    SgFunctionParameterTypeList *func_para_type_list =
        func->get_type()->get_argument_list();
    if (func_para_type_list->get_arguments().size() > 0)
      (tlist->get_arguments())
          .push_back(buildPointerType(buildPointerType(buildVoidType())));

    SgFunctionType *ftype =
        buildFunctionType(buildVoidType(), tlist); // func->get_type();
    //     SgFunctionType *ftype = deepCopy(func->get_type()); // why not just
    //     use the function type directly? deepCopy does not yet at this stage.
    string var_name = func->get_name().getString() + "p";
    // SgVariableDeclaration * ptofunc =
    // buildVariableDeclaration(var_name,buildPointerType(ftype), NULL,
    // src_global); prependStatement(ptofunc,src_global);
    SgVariableDeclaration *ptofunc =
        buildVariableDeclaration(var_name, buildPointerType(ftype), NULL,
                                 target_outlined_code->get_scope());

    // prependStatement(ptofunc,target_outlined_code);
    SageInterface::insertStatementBefore(target_outlined_code, ptofunc);
  }
  //   else
  //   Liao, 5/1/2009
  //   We still generate the prototype even they are not needed if dlopen() is
  //   used. since SageInterface::appendStatementWithDependentDeclaration()
  //   depends on it
  // if (SageInterface::is_Fortran_language() == false ) // C/C++ only
  if (use_dlopen == false &&
      SageInterface::is_Fortran_language() == false) // C/C++ only
  {
    // This is done in the original file (does not effect the separate file if
    // we outline the function there) Insert a single, global prototype (i.e., a
    // first non-defining declaration), which specifies the linkage property of
    // 'func'. insertGlobalPrototype (func, protos, src_global, target_func);

    sourceFileFunctionPrototype = insertGlobalPrototype(
        func, friendFunctionPrototypeList, src_global, target_func);

    SgFunctionDeclaration *source_first_nondef =
        sourceFileFunctionPrototype != NULL
            ? isSgFunctionDeclaration(sourceFileFunctionPrototype
                                          ->get_firstNondefiningDeclaration())
            : NULL;

    SgFunctionSymbol *sourceFileFunctionPrototypeSymbol =
        isSgFunctionSymbol(src_global->lookup_symbol(func->get_name()));
    if (sourceFileFunctionPrototypeSymbol == NULL &&
        source_first_nondef != NULL) {
      sourceFileFunctionPrototypeSymbol = isSgFunctionSymbol(
          src_global->find_symbol_from_declaration(source_first_nondef));
    }
    if (sourceFileFunctionPrototypeSymbol == NULL &&
        source_first_nondef != NULL) {
      sourceFileFunctionPrototypeSymbol = isSgFunctionSymbol(
          source_first_nondef->get_symbol_from_symbol_table());
    }
    if (sourceFileFunctionPrototypeSymbol == NULL &&
        source_first_nondef != NULL) {
      sourceFileFunctionPrototypeSymbol =
          new SgFunctionSymbol(source_first_nondef);
      src_global->insert_symbol(func->get_name(),
                                sourceFileFunctionPrototypeSymbol);
    }
    ROSE_ASSERT(sourceFileFunctionPrototypeSymbol != NULL);
    // Liao 12/6/2012. The assumption now is changed. A hidden nondefining
    // declaration is always created when a defining declaration is created. So
    // the hidden one is the first_nondefining decl associated with the function
    // symbol. The newly transformation generated function prototype is now
    // actually the 2nd prototype not directly associated with the func symbol
    ROSE_ASSERT(sourceFileFunctionPrototypeSymbol->get_declaration() ==
                sourceFileFunctionPrototype->get_firstNondefiningDeclaration());
    // ROSE_ASSERT(sourceFileFunctionPrototype->get_firstNondefiningDeclaration()
    // == sourceFileFunctionPrototype);
    if (Outliner::useNewFile != true) {
      ROSE_ASSERT(
          sourceFileFunctionPrototype->get_firstNondefiningDeclaration() !=
          sourceFileFunctionPrototype);
    }
    // Liao 12/6/2010, this assertion is not right. SageInterface function is
    // smart enough to automatically set the defining declaration for the
    // prototype DQ (2/27/2009): Assert this as a test!
    // ROSE_ASSERT(sourceFileFunctionPrototype->get_definingDeclaration() ==
    // NULL);
  }

  // This is the outlined function prototype that is put into the separate file
  // (when outlining is done to a separate file).
  SgFunctionDeclaration *outlinedFileFunctionPrototype = NULL;
  if (Outliner::useNewFile == true) {

    // DQ (9/25/2019): This is the correct function to use in the defered
    // evaluation data structure.

    // Build a function prototype and insert it first (will be at the top of the
    // generated file).
    // For template outlined functions we must build a matching template
    // prototype, otherwise wiring defining/nondefining will assert due to a
    // cross-variant definingDeclaration edge.
    outlinedFileFunctionPrototype = generatePrototype(func, scope, false);
    ROSE_ASSERT(outlinedFileFunctionPrototype != NULL);

    // Inherit defining function's inline property: avoid linking error when
    // linking multiple .lib files with the outlined functions
    if (func->get_functionModifier().isInline())
      outlinedFileFunctionPrototype->get_functionModifier().setInline();

    // DQ (3/17/2021): Get the first statement of the scope.
    // SgStatement* getFirstStatement(SgScopeStatement *scope,bool
    // includingCompilerGenerated=false);
    bool includingCompilerGenerated = false;
    SgStatement *firstStatement =
        SageInterface::getFirstStatement(scope, includingCompilerGenerated);
    ROSE_ASSERT(firstStatement != NULL);
    if (firstStatement != NULL) {
      // DQ (3/17/2021): When using the token-based unparsing we need to set
      // this so that the surrounding whitespace will be unparsed from the AST,
      // instead of the token stream.
      firstStatement->set_containsTransformationToSurroundingWhitespace(true);
    }
    // scope->append_declaration (outlinedFileFunctionPrototype);
    SageInterface::prependStatement(outlinedFileFunctionPrototype, scope);
    // DQ (11/9/2019): When used in conjunction with header file unparsing we
    // need to set the physical file id on entirety of the subtree being
    // inserted.
    SageBuilder::fixupSourcePositionFileSpecification(
        outlinedFileFunctionPrototype, filenameFromID);

    // DQ (3/17/2021): When using the token-based unparsing we need to set this
    // so that the surrounding whitespace will be unparsed from the AST, instead
    // of the token stream.
    outlinedFileFunctionPrototype
        ->set_containsTransformationToSurroundingWhitespace(true);
    // DQ (9/26/2019): Trying to trace down where there is a
    // SgFunctionParameterList with parent not being set!
    ROSE_ASSERT(
        outlinedFileFunctionPrototype->get_parameterList()->get_parent() !=
        NULL);

    // DQ (9/26/2019): Trying to trace down where there is a
    // SgFunctionParameterList with parent not being set!
    ROSE_ASSERT(func->get_parameterList()->get_parent() != NULL);

    // DQ (9/26/2019): check out the generate function prototype (parents of
    // some parts might not be set).
    SgFunctionParameterList *functionParameterList =
        outlinedFileFunctionPrototype->get_parameterList();
    ROSE_ASSERT(functionParameterList->get_parent() != NULL);

    // DQ (9/25/2019): The friend functions in the defered transformation
    // structure should be using outlinedFileFunctionPrototype instead.

    ROSE_ASSERT(headerFileTransformation.targetFriends.empty() == true);
    headerFileTransformation.targetFriends.push_back(
        outlinedFileFunctionPrototype);

    // The build function should have build symbol for the symbol table.
    SgFunctionSymbol *outlinedFileFunctionPrototypeSymbol =
        isSgFunctionSymbol(scope->lookup_symbol(func->get_name()));
    ROSE_ASSERT(outlinedFileFunctionPrototypeSymbol != NULL);
    // This is no longer true when a prototype is created when a defining one is
    // created.
    // ROSE_ASSERT(outlinedFileFunctionPrototypeSymbol->get_declaration() ==
    // outlinedFileFunctionPrototype);

    // The prototype must be connected to the defining declaration for the
    // outlined function. Some builder paths don't automatically wire this up,
    // especially for friend-related outlining.
    if (outlinedFileFunctionPrototype->get_definingDeclaration() == NULL) {
      outlinedFileFunctionPrototype->set_definingDeclaration(func);
    } else {
      // If a defining decl is already attached, it must match.
      ROSE_ASSERT(outlinedFileFunctionPrototype->get_definingDeclaration() ==
                  func);
    }

    // DQ (2/20/2009): ASK LIAO: If func is a defining declaration then
    // shouldn't the SageBuilder::buildNondefiningFunctionDeclaration() set the
    // definingDeclaration?
    // outlinedFileFunctionPrototype->set_definingDeclaration(func);
    outlinedFileFunctionPrototype->set_parent(scope);
    outlinedFileFunctionPrototype->set_scope(scope);

    // Set the func_prototype as the first non-defining declaration.
    // Liao 12/14/2012. the prototype is no longer the first non-defining one.
    // We have to explicit grab it.
    SgDeclarationStatement *first_non_def_decl =
        outlinedFileFunctionPrototype->get_firstNondefiningDeclaration();
    ROSE_ASSERT(first_non_def_decl != NULL);
    func->set_firstNondefiningDeclaration(first_non_def_decl);

    ROSE_ASSERT(outlinedFileFunctionPrototype->get_parent() != NULL);
    ROSE_ASSERT(
        outlinedFileFunctionPrototype->get_firstNondefiningDeclaration() !=
        NULL);
    ROSE_ASSERT(outlinedFileFunctionPrototype->get_definingDeclaration() !=
                NULL);

    // Add a message to the top of the outlined function that has been added
    SageInterface::addMessageStatement(outlinedFileFunctionPrototype,
                                       "/* OUTLINED FUNCTION PROTOTYPE */");

    // Make sure that internal referneces are to the same file (else the symbol
    // table information will not be consistant).
    ROSE_ASSERT(func->get_firstNondefiningDeclaration() != NULL);
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));
    ROSE_ASSERT(SageInterface::getEnclosingSourceFile(func->get_scope()) ==
                SageInterface::getEnclosingSourceFile(
                    func->get_firstNondefiningDeclaration()));
  } else {
    // if (!use_dlopen)
    {
      // Since the outlined function has been kept in the same file we can have
      // a pointer to the defining declaration.
      // sourceFileFunctionPrototype->set_definingDeclaration(func);
      // ROSE_ASSERT(sourceFileFunctionPrototype->get_definingDeclaration() !=
      // NULL);
      if (SageInterface::is_Fortran_language() == false) {
        ROSE_ASSERT(sourceFileFunctionPrototype != NULL);
        sourceFileFunctionPrototype->set_definingDeclaration(func);
        ROSE_ASSERT(sourceFileFunctionPrototype->get_definingDeclaration() !=
                    NULL);
      }
    }
  }

  ROSE_ASSERT(func->get_definition()->get_body()->get_parent() ==
              func->get_definition());
  // No forward declaration is needed for Fortran functions, Liao, 3/11/2009
  // if (SageInterface::is_Fortran_language() != true)
  ROSE_ASSERT(func->get_firstNondefiningDeclaration() != NULL);

  // DQ (8/15/2019): Adding support to defere the transformations in header
  // files (a performance improvement).
  return headerFileTransformation;

} // end Outliner::insert()

// eof
