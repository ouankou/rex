// DQ (6/24/2005): Not a very important bug in ROSE.
// This test code demonstrates the use of enums declarations with empty fileds.
// Basically "enum numbersEnum { } Numbers;" is unparsed as "enum numbersEnum Numbers;" which is an error.

// enum numbers;

// Initial defining declaration of enum
enum numbers {};
// redundent declaration (not output by ROSE)
enum numbers;

enum letters { a,b,c };
void foo(letters l);

// enum numbersEnum Numbers;
// enum numbersEnum { one,two } Numbers;
enum numbersEnum { } Numbers;
enum numbersEnum;
enum lettersEnum { x, y, z } Letters;

void foo (numbersEnum x);
void foo(enum numbersEnum x);
