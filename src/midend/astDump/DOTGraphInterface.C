
#include "DOTGraphInterface.h"

#include "DOTSubgraphRepresentationImpl.h"

#include <assert.h>

// Note that the DOTSubgraphRepresentationImpl.h contains
// a "using namespace std" declaration!  This is OK since
// that file is only used to support none header files
// and is not included in rose.h or other header files
// generally.

template class DOTSubgraphRepresentation<string>;
