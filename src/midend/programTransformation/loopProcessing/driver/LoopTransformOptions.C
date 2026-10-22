
#include <charconv>
#include <iostream>

#include "BlockingAnal.h"

#include "CommandOptions.h"

#include "CopyArrayAnal.h"

#include "FusionAnal.h"

#include "InterchangeAnal.h"

#include "LoopTransformOptions.h"

class DynamicTuning {
  static int dt;

public:
  static bool Do();
  static int increase_index() { return dt++; }
};

int DynamicTuning::dt = 0;
bool DynamicTuning::Do() {
  if (dt == 0) {
    dt = CmdOptions::GetInstance()->HasOption("-dt") ? 1 : -1;
  }
  return dt > 0;
}

inline unsigned ReadUnsignedInt(const std::vector<std::string> &argv,
                                unsigned &index, const std::string &name,
                                unsigned def_value) {
  if (index + 1 == argv.size())
    return def_value;
  const std::string &content = argv[index + 1];
  if (content.empty()) {
    std::cerr << "REX_LOOP_OPTION[unsigned-size]: " << name
              << " has an empty size\n";
    ROSE_ABORT();
  }
  const char *begin = content.data();
  const char *end = begin + content.size();
  if (begin != end && *begin == '+')
    ++begin;
  const bool numeric = begin != end && ((*begin >= '0' && *begin <= '9') ||
                                        (*begin == '-' && begin + 1 != end &&
                                         begin[1] >= '0' && begin[1] <= '9'));
  if (!numeric)
    return def_value;
  int value = 0;
  const auto parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end || value < 0) {
    std::cerr << "REX_LOOP_OPTION[unsigned-size]: " << name
              << " has invalid size '" << content << "'\n";
    ROSE_ABORT();
  }
  ++index;
  return static_cast<unsigned>(value);
}

inline unsigned ReadPositiveInt(const std::vector<std::string> &argv,
                                unsigned &index, const std::string &name,
                                unsigned def_value) {
  unsigned value = ReadUnsignedInt(argv, index, name, def_value);
  if (value == 0) {
    std::cerr << "REX_LOOP_OPTION[positive-size]: " << name
              << " requires a positive size\n";
    ROSE_ABORT();
  }
  return value;
}

class BlockOuterLoopOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetDefaultBlockSize(ReadPositiveInt(argv, index, "block size", 16));
    opt.SetBlockSel(new OuterLoopReuseBlocking(1));
  }

public:
  BlockOuterLoopOpt()
      : OptRegistryType("-bk1", " <blocksize> :block outer loops") {}
};

class BlockInnerLoopOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetDefaultBlockSize(ReadPositiveInt(argv, index, "block size", 16));
    opt.SetBlockSel(new InnerLoopReuseBlocking());
  }

public:
  BlockInnerLoopOpt()
      : OptRegistryType("-bk2", " <blocksize> :block inner loops") {}
};

class BlockAllLoopOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetDefaultBlockSize(ReadPositiveInt(argv, index, "block size", 16));
    opt.SetBlockSel(new AllLoopReuseBlocking());
  }

public:
  BlockAllLoopOpt()
      : OptRegistryType("-bk3", " <blocksize> :block all loops") {}
};

class CopyArrayDimensionOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    int dim = ReadUnsignedInt(argv, index, "array-copy dimension", 0);
    opt.SetCopySel(new CopyArrayUnderSizeLimit(dim));
  }

public:
  CopyArrayDimensionOpt()
      : OptRegistryType(
            "-cp",
            " <copydim> :copy array regions with dimensions <= <copydim>") {}
};

class ReuseInterchangeOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetInterchangeSel(new ArrangeReuseOrder());
  }

public:
  ReuseInterchangeOpt()
      : OptRegistryType("-ic1", " :loop interchange for more reuses") {}
};

class FissionOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetFusionSel(new LoopNestFusion());
  }

public:
  FissionOpt()
      : OptRegistryType("-fs0", " : maximum distribution at all loops") {}
};

class InnerFissionOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetFusionSel(new SameLevelFusion(new InnermostLoopFission()));
  }

public:
  InnerFissionOpt()
      : OptRegistryType("-fs01",
                        " : maximum distribution at inner-most loops") {}
};

class SingleReuseFusionOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetFusionSel(new SameLevelFusion(new AnyReuseFusionAnal()));
  }

public:
  SingleReuseFusionOpt()
      : OptRegistryType("-fs1", " :single-level loop fusion for more reuses") {}
};

class MultiReuseFusionOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    opt.SetFusionSel(new MultiLevelFusion(new AnyReuseFusionAnal()));
  }

public:
  MultiReuseFusionOpt()
      : OptRegistryType("-fs2", " :multi-level loop fusion for more reuses") {}
};

class SplitLimitOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    int limit = ReadUnsignedInt(argv, index, "split limit", 20);
    opt.SetTransAnalSplitLimit(limit);
  }

public:
  SplitLimitOpt()
      : OptRegistryType("-ta",
                        " <int> :split limit for transitive dep. analysis") {}
};

class CacheLineSizeOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    unsigned size = ReadPositiveInt(argv, index, "cache line size", 16);
    opt.SetCacheLineSize(size);
  }

public:
  CacheLineSizeOpt()
      : OptRegistryType("-clsize", " <int> :set cache line size") {}
};

class ReuseDistOpt : public LoopTransformOptions::OptRegistryType {
  virtual void operator()(LoopTransformOptions &opt, unsigned &index,
                          const std::vector<std::string> &argv) {
    unsigned size = ReadUnsignedInt(argv, index, "reuse distance", 4);
    opt.SetReuseDistance(size);
  }

public:
  ReuseDistOpt()
      : OptRegistryType("-reuse_dist", " <int> :set reuse distance") {}
};

LoopTransformOptions::LoopTransformOptions()
    : cpOp(0), cacheline(16), reuseDist(8), splitlimit(20),
      defaultblocksize(16) {
  icOp = new ArrangeOrigNestingOrder();
  fsOp = new SameLevelFusion(new OrigLoopFusionAnal());
  bkOp = new LoopNoBlocking();
}

LoopTransformOptions::~LoopTransformOptions() {
  delete icOp;
  delete fsOp;
  delete bkOp;
  delete cpOp;
}

void LoopTransformOptions::SetBlockSel(LoopBlocking *sel) {
  if (bkOp != 0)
    delete bkOp;
  bkOp = sel;
}

void LoopTransformOptions::SetCopySel(CopyArrayOperator *sel) {
  if (cpOp != 0)
    delete cpOp;
  cpOp = sel;
}
void LoopTransformOptions::SetInterchangeSel(ArrangeNestingOrder *sel) {
  delete icOp;
  icOp = sel;
}

void LoopTransformOptions::SetFusionSel(LoopNestFusion *sel) {
  delete fsOp;
  fsOp = sel;
}

LoopTransformOptions::OptType LoopTransformOptions::GetOptimizationType() {
  int t = (fsOp->GetOptimizationType() |
           ((bkOp == 0) ? NO_OPT : bkOp->GetOptimizationType()) |
           ((cpOp == 0) ? NO_OPT : cpOp->GetOptimizationType()) |
           icOp->GetOptimizationType());
  return (OptType)t;
}

LoopTransformOptions *LoopTransformOptions::inst = 0;
LoopTransformOptions *LoopTransformOptions::GetInstance() {
  if (inst == 0) {
    inst = new LoopTransformOptions();
    inst->RegisterOption(new BlockOuterLoopOpt);
    inst->RegisterOption(new BlockInnerLoopOpt);
    inst->RegisterOption(new BlockAllLoopOpt);
    inst->RegisterOption(new CopyArrayDimensionOpt);
    inst->RegisterOption(new ReuseInterchangeOpt);
    inst->RegisterOption(new FissionOpt);
    inst->RegisterOption(new InnerFissionOpt);
    inst->RegisterOption(new SingleReuseFusionOpt);
    inst->RegisterOption(new MultiReuseFusionOpt);
    inst->RegisterOption(new SplitLimitOpt);
    inst->RegisterOption(new CacheLineSizeOpt);
    inst->RegisterOption(new ReuseDistOpt);
  }
  return inst;
}

void LoopTransformOptions ::PrintUsage(std::ostream &stream) const {
  for (SinglyLinkedListWrap<OptRegistryType *>::Iterator p(reg); !p.ReachEnd();
       ++p) {
    stream << (*p)->GetName() << (*p)->GetExpl() << "\n";
  }
  stream << "-dt :perform dynamic tuning" << std::endl;
}

bool LoopTransformOptions ::DoDynamicTuning() const {
  return DynamicTuning::Do();
}

unsigned LoopTransformOptions ::GetDynamicTuningIndex() const {
  return DynamicTuning::increase_index();
}

void LoopTransformOptions ::RegisterOption(OptRegistryType *t) {
  reg.AppendLast(t);
}

void LoopTransformOptions ::SetOptions(const std::vector<std::string> &argvList,
                                       std::vector<std::string> *unknown_args) {
  for (unsigned index = 0; index < argvList.size(); ++index) {
    const std::string &opt_name = argvList[index];
    SinglyLinkedListWrap<OptRegistryType *>::Iterator p_registry(reg);
    for (; !p_registry.ReachEnd(); ++p_registry) {
      OptRegistryType *reg = *p_registry;
      if (opt_name == reg->GetName()) {
        (*reg)(*this, index, argvList);
        break;
      }
    }
    if (unknown_args != 0 && p_registry.ReachEnd())
      unknown_args->push_back(opt_name);
  }
}
