/**
 * Hook API include file
 *
 * Note to the reader:
 * This include defines two types of things: external functions and macros
 * Functions are used sparingly because a non-inlining compiler may produce
 * undesirable output.
 *
 * Find documentation here: https://xrpl-hooks.readme.io/reference/
 */

#ifndef HOOKAPI_INCLUDED
#define HOOKAPI_INCLUDED 1

#define KEYLET_HOOK 1
#define KEYLET_HOOK_STATE 2
#define KEYLET_ACCOUNT 3
#define KEYLET_AMENDMENTS 4
#define KEYLET_CHILD 5
#define KEYLET_SKIP 6
#define KEYLET_FEES 7
#define KEYLET_NEGATIVE_UNL 8
#define KEYLET_LINE 9
#define KEYLET_OFFER 10
#define KEYLET_QUALITY 11
#define KEYLET_EMITTED_DIR 12
#define KEYLET_TICKET 13
#define KEYLET_SIGNERS 14
#define KEYLET_CHECK 15
#define KEYLET_DEPOSIT_PREAUTH 16
#define KEYLET_UNCHECKED 17
#define KEYLET_OWNER_DIR 18
#define KEYLET_PAGE 19
#define KEYLET_ESCROW 20
#define KEYLET_PAYCHAN 21
#define KEYLET_EMITTED 22

#define COMPARE_EQUAL 1U
#define COMPARE_LESS 2U
#define COMPARE_GREATER 4U

#include "error.h"
#include "extern.h"
#include "sfcodes.h"
#include "macro.h"

#endif
