#ifndef USDK_H
#define USDK_H

/* Umbrella header - includes every public USDK header. A consumer that
 * only needs one piece (e.g. just usdk/candidate.h) is free to include
 * that directly instead; nothing in this project requires going through
 * this umbrella. */

#include "usdk/platform.h"
#include "usdk/types.h"
#include "usdk/status.h"
#include "usdk/candidate.h"
#include "usdk/vote.h"
#include "usdk/wire.h"
#include "usdk/plugin.h"
#include "usdk/manifest.h"
#include "usdk/ffi.h"
#include "usdk/core.h"
#include "usdk/perceive.h"
#include "usdk/deliberate.h"
#include "usdk/verify.h"

#endif /* USDK_H */
