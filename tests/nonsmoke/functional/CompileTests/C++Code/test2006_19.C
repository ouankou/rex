

static void write_tok_str(char *str)
   {
   }

static void gen_asm_decl(void)
   {
     write_tok_str(false ? "__asm(" : "asm(");
   }

