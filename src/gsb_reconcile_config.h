#ifndef _GSB_RECONCILE_CONFIG_H
#define _GSB_RECONCILE_CONFIG_H (1)

#include <gtk/gtk.h>

enum reconciliation_columns {
    RECONCILIATION_NAME_COLUMN = 0,
    RECONCILIATION_INIT_DATE_COLUMN,
    RECONCILIATION_FINAL_DATE_COLUMN,
    RECONCILIATION_INIT_BALANCE_COLUMN,
    RECONCILIATION_FINAL_BALANCE_COLUMN,
    RECONCILIATION_ACCOUNT_COLUMN,
    RECONCILIATION_RECONCILE_COLUMN,
    RECONCILIATION_WEIGHT_COLUMN,
    NUM_RECONCILIATION_COLUMNS
};


/* START_INCLUDE_H */
/* END_INCLUDE_H */

/* START_DECLARATION */
GtkWidget *gsb_reconcile_config_create ( void );
void gsb_reconcile_config_fill ( void );
/* END_DECLARATION */
#endif
