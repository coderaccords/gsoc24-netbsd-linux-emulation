/* $NetBSD: db_machdep.h,v 1.3 2005/12/11 12:16:04 christos Exp $ */

#include <arm/db_machdep.h>

/* acorn26 uses ELF */
#undef DB_AOUT_SYMBOLS
#define DB_ELF_SYMBOLS
#define DB_ELFSIZE 32

void db_show_panic_cmd	__P((db_expr_t, int, db_expr_t, const char *));
void db_show_frame_cmd	__P((db_expr_t, int, db_expr_t, const char *));
void db_bus_write_cmd	__P((db_expr_t, int, db_expr_t, const char *));
void db_irqstat_cmd	__P((db_expr_t, int, db_expr_t, const char *));
