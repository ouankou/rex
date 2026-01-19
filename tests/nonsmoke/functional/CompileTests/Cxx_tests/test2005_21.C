/*

Hi Dan,

As we talked about on the phone, I have written a small main function showing
that copying the SgGlobal object works well and the deep copy mechanism seems to
work. (I can output the original SgProject and the modification done to the copy
of it (copy from SgGlobal) doesn't affect the original one. But I cannot output
the copied object, so I cannot tell how the copy is "behaving".)

As I also want to output/unparse the copy of the SgGlobal object:
Is it the right approach to add the SgGlobal root to the file  (like
file.set_root(root_copy);)? And then unparse the file/sgproject? When I do that,
I only get an error message at runtime saying:
home/hauge2/ROSE/src/backend/unparser/unparse_stmt.C:1047: void
Unparser::unparseFuncDefnStmt(SgStatement*, SgUnparse_Info&): Assertion
`funcdefn_stmt->get_declaration() != __null' failed.

Or is there another way of outputting the copied part?


I'm attaching the small main function (and a small testfile) with some comments
showing what is working and what is not working. In summary this is: Not
working:
- SgProject::copy and SgFile::copy (most likely for the same reason, the copy
constructor of SgFile)
- unparsing a file/sgproject after setting the modified copy of the SgGlobal as
the root of the file.

Working:
- SgGlobal::copy and the deep copy mechanism.


--
Vera

*/
