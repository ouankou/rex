
enum AVSampleFormat
   {
     AV_SAMPLE_FMT_NONE = -1,
     AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
     AV_SAMPLE_FMT_FLTP,        ///< float, planar
     AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
   };

typedef struct AVCodec
   {
     const char *name;
     const enum AVSampleFormat *sample_fmts; ///< array of supported sample formats, or NULL if unknown, array is terminated by -1
   } AVCodec;

void f(const enum AVSampleFormat[]);

int main()
   {
     const enum AVSampleFormat a[]= { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };

     const enum AVSampleFormat *sample_fmts_alt = a;

     return 0;
   }
