

static void write_tok_str(char *str)
   {
   }

static void gen_asm_decl(void)
   {
  // DQ (8/14/2006): This was allowed by g++ but not by legacy frontend, it was
  // fixed directly in the legacy frontend source code when we worked on using
  // the Intel compiler with ROSE (compiling both legacy frontend source and
  // ROSE source using the Intel icpc compiler). As a result this is a little
  // less critical to fix, plus there is little to do about it since the issue
  // is with legacy frontend, and I think that legacy frontend is correct in
  // this case and g++ is being lax on the subject of implicit casting of const.

  // AS and PC (060206) changed to cast away const (example where this is fixed
  // in legacy frontend. write_tok_str(false ? "__asm(" : "asm(");
  write_tok_str(false ? (char *)"__asm(" : (char *)"asm(");
   }

