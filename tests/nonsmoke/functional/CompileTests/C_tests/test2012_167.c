
typedef enum _HTDirShow {
  HT_DS_SIZE = 0x1,
  HT_DS_DATE = 0x2,
} HTDirShow;

// This is unparsed as: static HTDirShow dir_show = _HTDirShow(27);
// static HTDirShow dir_show = HT_DS_SIZE+HT_DS_DATE+HT_DS_DES+HT_DS_ICON;
static HTDirShow dir_show = HT_DS_SIZE+HT_DS_DATE;
