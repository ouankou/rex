

typedef struct
   {
  // int ks1;

  // void (*stream)(const unsigned char *in, unsigned char *out, const unsigned char iv[16]);

  // Original code is:
  // void (*stream)(const unsigned char *in, unsigned char iv[16]);
  // unparser generates:
  // void (*stream)(const unsigned char *, unsigned char ()[16]);
     void (*stream)(const unsigned char *in, unsigned char iv[16]);

} EVP_AES_XTS_CTX;

// Original code is:
//    void (*stream)(const unsigned char *in, unsigned char iv[16]);
// unparser generates:
//    void (*stream)(const unsigned char *, unsigned char ()[16]);
      void (*stream)(const unsigned char *in, unsigned char iv[16]);


// Another example with const used.
void (*stream2)(const unsigned char *in, const unsigned char iv[16]);

