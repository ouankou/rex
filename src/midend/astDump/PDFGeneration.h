// Author: Markus Schordan

#ifndef PDFGENERATION_H
#define PDFGENERATION_H

#include "AstProcessing.h"

#include <hpdf.h>
class PDFInheritedAttribute {
public:
  explicit PDFInheritedAttribute(HPDF_Outline currentOutline,
                                 HPDF_Destination parentPage)
      : currentOutline(currentOutline), parentPage(parentPage) {}
  PDFInheritedAttribute() : currentOutline(NULL), parentPage(NULL) {}
  HPDF_Outline currentOutline;
  HPDF_Destination parentPage;
};

class PDFGeneration : public SgTopDownProcessing<PDFInheritedAttribute> {
public:
  PDFGeneration()
      : pdfFile(nullptr), theFont(nullptr), currentPageNumber(0),
        currentPage(nullptr) {}
  virtual void generate(std::string filename, SgNode *node);

protected:
  virtual PDFInheritedAttribute
  evaluateInheritedAttribute(SgNode *node,
                             PDFInheritedAttribute inheritedValue);
  HPDF_Doc pdfFile;
  HPDF_Font theFont;
  std::vector<HPDF_Page> pages;
  std::vector<HPDF_Destination> pageDests;
  std::string filename;
  size_t currentPageNumber;
  HPDF_Page currentPage;
  HPDF_Rect fontBBox;

  virtual void edit_page(size_t pageNumber, SgNode *node,
                         PDFInheritedAttribute inheritedValue);
  virtual std::string get_bookmark_name(SgNode *node);
  void create_textlink(const std::string &text, HPDF_Destination target,
                       int hitboxextender = 0);
  void pdf_setup(std::string filename, size_t numPages);
  void pdf_finalize();
  void begin_page();
  void end_page();
};

#endif
