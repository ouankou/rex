

// Author: Markus Schordan
// $Id: AstPDFGeneration.C,v 1.5 2007/02/19 16:08:38 vuduc2 Exp $

#ifndef ASTPDFGENERATION_C
#define ASTPDFGENERATION_C

#include "AstNodeVisitMapping.h"

#include "roseInternal.h"

#include "sage3basic.h"

#include <assert.h>

#include <iostream>

#include <cstring>

#include <sstream>

#include "AstPDFGeneration.h"

#include "PDFGeneration.h"

// DQ (12/31/2005): This is OK if not declared in a header file
using namespace std;

class AstPDFGeneration_private : public PDFGeneration {
public:
  virtual void generate(std::string filename, SgNode *node);
  virtual void generate(SgProject *projectNode);
  void generateInputFiles(SgProject *projectNode);
  void generateWithinFile(const std::string &filename, SgFile *node); // ****
  void generateWithinFile(SgFile *node);                              // ****
protected:
  // PDFInheritedAttribute evaluateInheritedAttribute(SgNode* node,
  // PDFInheritedAttribute inheritedValue); -- using parent's version
  virtual std::string get_bookmark_name(SgNode *node);
  void edit_page(size_t pageNumber, SgNode *node,
                 PDFInheritedAttribute inheritedValue);
  AstNodeVisitMapping::MappingType addrPageMapping;
};

void AstPDFGeneration::generate(SgProject *projectNode) {
  AstPDFGeneration_private p;
  p.generate(projectNode);
}

void AstPDFGeneration::generate(std::string filename, SgNode *node) {
  AstPDFGeneration_private p;
  p.generate(filename, node);
}

void AstPDFGeneration::generateInputFiles(SgProject *projectNode) {
  AstPDFGeneration_private p;
  p.generateInputFiles(projectNode);
}

void AstPDFGeneration::generateWithinFile(const std::string &filename,
                                          SgFile *node) {
  AstPDFGeneration_private p;
  p.generateWithinFile(filename, node);
}

void AstPDFGeneration::generateWithinFile(SgFile *node) {
  AstPDFGeneration_private p;
  p.generateWithinFile(node);
}

void AstPDFGeneration_private::generate(string filename, SgNode *node) {
  string pdffilename = filename + ".pdf";
  // cout << "generating PDF file: " << pdffilename << " ... ";
  AstNodeVisitMapping addrPageMappingTrav(0);
  addrPageMappingTrav.traverse(node, preorder);
  size_t numPages = addrPageMappingTrav.pagenum;
  addrPageMapping = addrPageMappingTrav.address_pagenum;
  pdf_setup(pdffilename, numPages);
  PDFInheritedAttribute pdfIA(pdfFile);
  traverse(node, pdfIA);
  pdf_finalize();
  // cout << "done (full AST)." << endl;
}

// Full AST for the entire project, one pdf file for each input source file
// Liao, 8/19/2016
void AstPDFGeneration_private::generate(SgProject *projectNode) {
  const SgFilePtrList &fList = projectNode->get_fileList();
  for (SgFilePtrList::const_iterator fl_iter = fList.begin();
       fl_iter != fList.end(); fl_iter++) {
    ROSE_ASSERT(*fl_iter != 0);
    SgFile *fp = *fl_iter;
    std::string filename = fp->getFileName();
    filename = Rose::StringUtility::stripPathFromFileName(filename);
    generate(filename, *fl_iter);
  }
}

void AstPDFGeneration_private::generateInputFiles(SgProject *projectNode) {
  const SgFilePtrList &fList = projectNode->get_fileList();
  for (SgFilePtrList::const_iterator fl_iter = fList.begin();
       fl_iter != fList.end(); fl_iter++) {
    ROSE_ASSERT(*fl_iter != 0);
    generateWithinFile(*fl_iter);
  }
}

void AstPDFGeneration_private::generateWithinFile(const string &pdffilename,
                                                  SgFile *node) {
  AstNodeVisitMapping addrPageMappingTrav(1);
  addrPageMappingTrav.traverseWithinFile(node, preorder);
  addrPageMapping = addrPageMappingTrav.address_pagenum;
  // cout << "generating PDF file: " << pdffilename << " ... ";
  pdf_setup(pdffilename + ".pdf", addrPageMappingTrav.pagenum);
  PDFInheritedAttribute pdfIA(pdfFile);
  // traverse/*WithinFile*/(node,pdfIA);
  //  tps (01/05/08) changed this call from traverse to traverseWithinFile
  traverseWithinFile(node, pdfIA);
  pdf_finalize();
  // cout << "done." << endl;
}

void AstPDFGeneration_private::generateWithinFile(SgFile *node) {
  // string
  // pdffilename=string("./")+string(Rose::utility_stripPathFromFileName(Rose::getFileName(node)));
  string pdffilename =
      string("./") +
      string(Rose::utility_stripPathFromFileName(node->getFileName()));
  generateWithinFile(pdffilename, node);
}

string AstPDFGeneration_private::get_bookmark_name(SgNode *node) {
  string nodefilename = "--not initialized--";
  string bookmarktext;
  {
    ostringstream ss;
    ss << node->sage_class_name() << " ";
    SgLocatedNode *sgLocNode = dynamic_cast<SgLocatedNode *>(node);
    if (sgLocNode) {
      Sg_File_Info *fi = sgLocNode->get_file_info();
      if (fi) {
        // ss << "(" << fi->get_line() << "," << fi->get_col() << ") in \"" <<
        // fi->get_filename() << "\""; nodefilename=string(fi->get_filename());
        //  if (fi->isCompilerGenerated())
        //    ss << " compilerGenerated ";
        if (fi->isTransformation())
          ss << " transformation ";
        if (fi->isOutputInCodeGeneration())
          ss << " unparsable ";
        ss << "(" << fi->get_line() << "," << fi->get_col() << ") in \""
           << fi->get_filename() << "\"";
        nodefilename = string(fi->get_filename());
      } else {
        ss << "(BUG)";
      }
    } else {
      ss << ""; // provide no explicit info about the lack of file_info
    }
    bookmarktext = ss.str();
  }

  return bookmarktext;
}

void AstPDFGeneration_private::edit_page(size_t pageNumber, SgNode *node,
                                         PDFInheritedAttribute inheritedValue) {
  // JW (added by DQ 7/23/2004): Adds address of each IR node to the top of the
  // page
  HPDF_Page_SetRGBFill(currentPage, 1, 0, 0);
  // DQ (1/20/2006): Modified for 64 bit machines
  // ostringstream _ss; _ss << "0x" << std::hex << (int)node;
  ostringstream _ss;
  _ss << "pointer:" << std::hex << node;
  HPDF_Page_ShowTextNextLine(currentPage, _ss.str().c_str());

  // Liao, 2/11/2009, move some essential information from bookmark into the
  // page also since some deep levels of bookmarks cannot display long lines
  // properly by acrobat reader
  HPDF_Page_ShowTextNextLine(currentPage, node->sage_class_name());
  SgLocatedNode *sgLocNode = dynamic_cast<SgLocatedNode *>(node);
  if (sgLocNode) {
    Sg_File_Info *fi = sgLocNode->get_file_info();
    ostringstream temp;
    temp << fi->get_filename() << " " << fi->get_line() << ":" << fi->get_col();
    if (fi != NULL)
      HPDF_Page_ShowTextNextLine(currentPage, temp.str().c_str());
    assert(fi != NULL);
    if (fi->isTransformation())
      HPDF_Page_ShowTextNextLine(currentPage, "IsTransformation:1");
    else
      HPDF_Page_ShowTextNextLine(currentPage, "IsTransformation:0");

    if (fi->isOutputInCodeGeneration())
      HPDF_Page_ShowTextNextLine(currentPage, "IsOutputInCodeGeneration:1");
    else
      HPDF_Page_ShowTextNextLine(currentPage, "IsOutputInCodeGeneration:0");
  }
  if (SgDeclarationStatement *declaration = isSgDeclarationStatement(node);
      declaration != nullptr && declaration->has_semantic_mangled_name()) {
    HPDF_Page_ShowTextNextLine(currentPage,
                               ("Declaration mangled name: " +
                                declaration->get_mangled_name().getString())
                                   .c_str());
  }

  // Expression types can contain context-dependent template arguments. Supply
  // the exact statement use-site instead of asking a shared type node to infer
  // a lexical context it does not own.
  if (SgExpression *expression = isSgExpression(node);
      expression != nullptr && expression->has_semantic_value_type()) {
    if (isSgTypeUnknown(expression->get_type()) != nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[pdf-expression-type]: expression=%p type=%s "
              "parent=%p parent-type=%s file=%s line=%d owns SgTypeUnknown\n",
              static_cast<void *>(expression), expression->class_name().c_str(),
              static_cast<void *>(expression->get_parent()),
              expression->get_parent() != nullptr
                  ? expression->get_parent()->class_name().c_str()
                  : "<null>",
              expression->get_file_info() != nullptr
                  ? expression->get_file_info()->get_filenameString().c_str()
                  : "<unknown>",
              expression->get_file_info() != nullptr
                  ? expression->get_file_info()->get_line()
                  : 0);
      ROSE_ABORT();
    }
    SgStatement *use_site = SageInterface::getEnclosingStatement(expression);
    Sg_File_Info *use_primary =
        use_site != nullptr ? use_site->get_file_info() : nullptr;
    Sg_File_Info *use_start =
        use_site != nullptr ? use_site->get_startOfConstruct() : nullptr;
    Sg_File_Info *use_end =
        use_site != nullptr ? use_site->get_endOfConstruct() : nullptr;
    SgSourceFile *source_file =
        use_site != nullptr ? SageInterface::getEnclosingSourceFile(use_site)
                            : nullptr;
    SgScopeStatement *scope =
        use_site != nullptr ? SageInterface::getScope(use_site) : nullptr;
    if (use_site == nullptr || use_primary == nullptr || use_start == nullptr ||
        use_end == nullptr || source_file == nullptr || scope == nullptr) {
      fprintf(stderr,
              "REX_AST_INVARIANT[pdf-expression-type]: expression=%p type=%s "
              "has no exact statement provenance, source file, or semantic "
              "scope\n",
              static_cast<void *>(expression),
              expression->class_name().c_str());
      ROSE_ABORT();
    }
    const bool primary_output = use_primary->isOutputInCodeGeneration();
    const bool start_output = use_start->isOutputInCodeGeneration();
    const bool end_output = use_end->isOutputInCodeGeneration();
    if (primary_output != start_output || start_output != end_output) {
      fprintf(stderr,
              "REX_AST_INVARIANT[pdf-expression-type]: statement=%p type=%s "
              "has inconsistent primary/start/end output role=%d/%d/%d\n",
              static_cast<void *>(use_site), use_site->class_name().c_str(),
              primary_output ? 1 : 0, start_output ? 1 : 0, end_output ? 1 : 0);
      ROSE_ABORT();
    }
    SgDeclarationStatement *use_declaration =
        isSgDeclarationStatement(use_site);
    const bool semantic_auxiliary =
        use_declaration != nullptr &&
        isSgAuxiliaryDeclarationList(use_declaration->get_parent()) != nullptr;
    const bool semantic_nonreal =
        isSgNonrealDecl(use_declaration) != nullptr &&
        isSgDeclarationScope(use_declaration->get_parent()) != nullptr;
    if (semantic_auxiliary) {
      isSgAuxiliaryDeclarationList(use_declaration->get_parent())
          ->validate_semantic_non_output_role();
    }
    // An expression's value type is semantic identity, not a source type-use.
    // In particular, implicit casts have no lexical TypeLoc and therefore no
    // valid name-qualification record.  Diagnostic renderers must report the
    // typed IR directly instead of asking the source unparser to invent a type
    // spelling at the enclosing statement.
    const char *role =
        primary_output && !semantic_auxiliary && !semantic_nonreal
            ? "source-owned expression"
            : "non-output expression";
    HPDF_Page_ShowTextNextLine(
        currentPage, ("Expression semantic type: " +
                      expression->get_type()->class_name() + " (" + role + ")")
                         .c_str());
  }

  HPDF_Page_SetRGBFill(currentPage, 0, 1, 0);

  if (inheritedValue.parentPage == NULL) {
    HPDF_Page_ShowTextNextLine(currentPage, "");
    HPDF_Page_ShowTextNextLine(currentPage, "      root node");
  } else {
    HPDF_Page_ShowTextNextLine(currentPage, "");
    HPDF_Page_ShowTextNextLine(currentPage, "     ");
    create_textlink("Click here to go to the parent node",
                    inheritedValue.parentPage, 9);
  }
  HPDF_Page_ShowTextNextLine(currentPage, "");
  HPDF_Page_ShowTextNextLine(currentPage, "");

  // generate RTI information for SgNode
  {
    RTIReturnType rti = node->roseRTI();
    for (RTIReturnType::iterator i = rti.begin(); i < rti.end(); i++) {
      if (strlen(i->type) >= 7 && strncmp(i->type, "static ", 7) == 0) {
        continue; // Skip static members
      }
      HPDF_Page_SetRGBFill(currentPage, 0.5, 0, 0.1);
      HPDF_Page_ShowTextNextLine(currentPage, i->type);
      HPDF_Page_ShowText(currentPage, " ");
      HPDF_Page_SetRGBFill(currentPage, 0.0, 0.5, 0.5);
      HPDF_Page_ShowText(currentPage, i->name);
      HPDF_Page_ShowText(currentPage, " : ");
      HPDF_Page_SetRGBFill(currentPage, 0.0, 0.0, 0.0);

      string value = i->value;
      if (value.size() >=
          80) { // HPDF doesn't like strings > 64k, and we probably shouldn't be
                // trying to print them anyway; this trims things to a
                // reasonable length
        value = "<too long>: " + value.substr(0, 80);
      }
      if (value.size() >= 80) {
        value = "<too long>: " + value.substr(0, 80);
      }
      AstNodeVisitMapping::MappingType::iterator mapit;

      // ensure that mapping value exists (otherwise it would be added to the
      // map) and decide whether to create a link to a page (representing a
      // node) or not
      mapit = addrPageMapping.find(value);
      if (mapit != addrPageMapping.end()) {
        size_t destPageNum = mapit->second;
        ROSE_ASSERT(destPageNum < pageDests.size());
        create_textlink(value.c_str(), pageDests[destPageNum] /* targetpage */);
      } else {
        HPDF_Page_ShowText(currentPage, value.c_str());
      }
    }
  }

  // generate AstAttribute information
  {
    // printf ("In AstPDFGeneration_private::edit_page(): using new attribute
    // interface \n"); if (node->get_attribute() != NULL) DQ (4/10/2006): New
    // AstAttribute Interface (but access the AstAttributeMechanism directly
    // since "getAttributeIdentifiers()" is not in the interface implemented at
    // the IR nodes).
    if (node->get_attributeMechanism() != NULL) {
      AstAttributeMechanism::AttributeIdentifiers aidents =
          node->get_attributeMechanism()->getAttributeIdentifiers();
      HPDF_Page_ShowTextNextLine(currentPage, ""); // next line
      HPDF_Page_SetRGBFill(currentPage, 0.0, 0.2, 0.7);
      HPDF_Page_ShowTextNextLine(currentPage, "AstAttributes:"); // next line
      for (AstAttributeMechanism::AttributeIdentifiers::iterator it =
               aidents.begin();
           it != aidents.end(); it++) {
        HPDF_Page_ShowTextNextLine(currentPage, ""); // next line
        HPDF_Page_SetRGBFill(currentPage, 0.0, 0.2, 0.7);
        HPDF_Page_ShowText(currentPage, ((*it) + ": ").c_str());
        HPDF_Page_SetRGBFill(currentPage, 0.0, 0.0, 0.0);
        // float textxpos=PDF_get_value(pdfFile,"textx",0.0);
        // float textypos=PDF_get_value(pdfFile,"texty",0.0);
        // string attributeValue = (node->attribute()[*it])->toString();
        string attributeValue = node->getAttribute(*it)->toString();

        // split string into different substrings separated by newlines
        string substring = "";
        int oldpos = 0;
        int newpos;
        do {
          newpos = attributeValue.find('\n', oldpos);
          substring = attributeValue.substr(oldpos, newpos - oldpos);
          if (oldpos == 0)
            HPDF_Page_ShowText(currentPage, substring.append("   ").c_str());
          else
            HPDF_Page_ShowTextNextLine(currentPage, substring.c_str());
          oldpos = newpos + 1; // go to next '\n' and skip it
        }
        // DQ (8/9/2005): Suggested fix from Rich (fixes infinite loop where AST
        // Attributes are attached) while( newpos < (int)
        // attributeValue.size());
        while (newpos < (int)attributeValue.size() && newpos >= 0);
        // PDF_show_boxed(pdfFile, attributeValue.c_str(), textxpos, textypos,
        // 0, 0, "left", "");
      }
    } // end if
  }
}

#endif
