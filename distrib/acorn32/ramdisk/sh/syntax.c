/*
 * This file was generated by the mksyntax program.
 */

#include "shell.h"
#include "syntax.h"

/* syntax table used when not in quotes */
const char basesyntax[257] = {
      CEOF,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CSPCL,   CNL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CSPCL,   CWORD,   CDQUOTE,
      CWORD,   CVAR,    CWORD,   CSPCL,
      CSQUOTE, CSPCL,   CSPCL,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CSPCL,   CSPCL,   CWORD,   CSPCL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CBACK,   CWORD,   CWORD,
      CWORD,   CBQUOTE, CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CSPCL,   CENDVAR, CWORD,
      CWORD,   CWORD,   CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD
};

/* syntax table used when in double quotes */
const char dqsyntax[257] = {
      CEOF,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CNL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CCTL,    CENDQUOTE,
      CWORD,   CVAR,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CWORD,   CWORD,   CCTL,    CWORD,
      CCTL,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CWORD,   CWORD,   CCTL,    CWORD,
      CCTL,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL,    CBACK,   CWORD,   CWORD,
      CWORD,   CBQUOTE, CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CENDVAR, CCTL,
      CWORD,   CWORD,   CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD
};

/* syntax table used when in single quotes */
const char sqsyntax[257] = {
      CEOF,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CNL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CCTL,    CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CENDQUOTE,CWORD,  CWORD,   CCTL,
      CWORD,   CWORD,   CCTL,    CWORD,
      CCTL,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CWORD,   CWORD,   CCTL,    CWORD,
      CCTL,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CCTL,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CCTL,
      CWORD,   CWORD,   CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD
};

/* syntax table used when in arithmetic */
const char arisyntax[257] = {
      CEOF,    CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CNL,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CDQUOTE,
      CWORD,   CVAR,    CWORD,   CWORD,
      CSQUOTE, CLP,     CRP,     CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CBACK,   CWORD,   CWORD,
      CWORD,   CBQUOTE, CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CENDVAR, CWORD,
      CWORD,   CWORD,   CCTL,    CCTL,
      CCTL,    CCTL,    CCTL,    CCTL,
      CCTL,    CCTL,    CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD,   CWORD,   CWORD,   CWORD,
      CWORD
};

/* character classification table */
const char is_type[257] = {
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       ISSPECL, 0,
      ISSPECL, ISSPECL, 0,       0,
      0,       0,       0,       ISSPECL,
      0,       0,       ISSPECL, 0,
      0,       ISDIGIT, ISDIGIT, ISDIGIT,
      ISDIGIT, ISDIGIT, ISDIGIT, ISDIGIT,
      ISDIGIT, ISDIGIT, ISDIGIT, 0,
      0,       0,       0,       0,
      ISSPECL, ISSPECL, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      ISUPPER, ISUPPER, ISUPPER, ISUPPER,
      0,       0,       0,       0,
      ISUNDER, 0,       ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      ISLOWER, ISLOWER, ISLOWER, ISLOWER,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0,       0,       0,       0,
      0
};
