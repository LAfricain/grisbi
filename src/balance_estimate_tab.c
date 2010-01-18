/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C) 2007 Dominique Parisot                                   */
/*          zionly@free.org                                                   */
/*          2008-2010 Pierre Biava (grisbi@pierre.biava.name)                 */
/*          http://www.grisbi.org                                             */
/*                                                                            */
/*  This program is free software; you can redistribute it and/or modify      */
/*  it under the terms of the GNU General Public License as published by      */
/*  the Free Software Foundation; either version 2 of the License, or         */
/*  (at your option) any later version.                                       */
/*                                                                            */
/*  This program is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*  GNU General Public License for more details.                              */
/*                                                                            */
/*  You should have received a copy of the GNU General Public License         */
/*  along with this program; if not, write to the Free Software               */
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                            */
/* ************************************************************************** */

/* ./configure --with-balance-estimate */

/*
 * prefix bet : Balance Estimate Tab
 *
 * TODO : change the color of each line in the graph :
 * red if balance is less than 0.
 * orange if balance is less than the minimum desired balance.
 * TODO : add a select button to display the selected line in the array
 * in the scheduler tab or in the account tab.
 */

#include "include.h"
#include <config.h>

#ifdef ENABLE_BALANCE_ESTIMATE

/*START_INCLUDE*/
#include "balance_estimate_tab.h"
#include "./utils_dates.h"
#include "./gsb_data_account.h"
#include "./gsb_data_budget.h"
#include "./gsb_data_category.h"
#include "./gsb_data_payee.h"
#include "./gsb_data_scheduled.h"
#include "./gsb_data_transaction.h"
#include "./gsb_real.h"
#include "./gsb_scheduler.h"
#include "./gsb_transactions_list_sort.h"
#include "./main.h"
#include "./include.h"
#include "./structures.h"
#include "./traitement_variables.h"
/*END_INCLUDE*/


struct _bet_range
{
    gboolean first_pass;
    GDate *min_date;
    GDate *max_date;
    gsb_real min_balance;
    gsb_real max_balance;
    gsb_real current_balance;
};

struct _historical
{
    gint div;
	gint account_nb;
    SBR	*sbr;
	GHashTable *list_sub_div;
};


/*START_STATIC*/
static void bet_account_selection_changed (GtkTreeSelection *treeselection, gpointer user_data);
static gint bet_date_sort_function (GtkTreeModel *model, GtkTreeIter *itera, GtkTreeIter *iterb, gpointer user_data);
static void bet_duration_button_clicked ( GtkWidget *togglebutton, GtkWidget *spin_button );
static void bet_historical_data_clicked ( GtkWidget *togglebutton, gpointer data );
static void bet_estimate_refresh (void);
static gboolean bet_update_average_column (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gboolean bet_update_graph (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static gint bet_date_sort_function (GtkTreeModel *model,
                        GtkTreeIter *a,
                        GtkTreeIter *b,
                        gpointer user_data);
static void bet_create_parameter_page ( GtkWidget *notebook );
static void bet_create_array_page ( GtkWidget *notebook );
static void bet_create_graph_page ( GtkWidget *notebook );
static void bet_create_historical_data_page ( GtkWidget *notebook );
static GtkWidget *bet_estimate_get_list_accounts ( GtkWidget *container );
static GtkWidget *bet_estimate_get_duration_widget ( GtkWidget *container );
static gboolean bet_duration_number_changed ( GtkWidget *spin_button, GtkWidget *togglebutton );
static void bet_duration_period_clicked ( GtkWidget *togglebutton, gpointer data );
static GtkWidget *bet_estimate_get_historical_data ( GtkWidget *container );
static void bet_estimate_populate_historical_data ( void );
static gboolean bet_estimate_populate_div ( gint transaction_number,
                        GHashTable  *list_div );
static gboolean bet_estimate_update_div ( SH *sh, gint transaction_number, gint sub_div );
static gboolean bet_estimate_affiche_div ( GHashTable  *list_div, GtkWidget *tree_view );
static void bet_estimate_populate_div_model ( gpointer key,
                        gpointer value,
                        gpointer user_data);
static SBR *initialise_struct_bet_range ( void );
static SH *initialise_struct_historical ( void );
static void free_struct_bet_range ( SBR *sbr );
static void free_struct_historical ( SH *sh );
/*END_STATIC*/

/*START_EXTERN*/
extern gboolean balances_with_scheduled;
extern gsb_real null_real;
extern GtkWidget *window;
/*END_EXTERN*/

/* pointeurs définis en fonction du type de données catégories ou IB */
gint (*ptr_div) ( gint transaction_num );
gint (*ptr_sub_div) ( gint transaction_num );
gchar* (*ptr_div_name) ( gint div_num, gint sub_div, const gchar *return_value_error );

enum bet_account_tree_columns {
    SPP_ACCOUNT_TREE_NAME_COLUMN,
    SPP_ACCOUNT_TREE_NUM_COLUMNS
};


enum bet_estimation_tree_columns {
    SPP_ESTIMATE_TREE_DATE_COLUMN,
    SPP_ESTIMATE_TREE_DESC_COLUMN,
    SPP_ESTIMATE_TREE_DEBIT_COLUMN,
    SPP_ESTIMATE_TREE_CREDIT_COLUMN,
    SPP_ESTIMATE_TREE_BALANCE_COLUMN,
    SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
    SPP_ESTIMATE_TREE_AMOUNT_COLUMN,	/* the amount without currency */
    SPP_ESTIMATE_TREE_BALANCE_COLOR,
    SPP_ESTIMATE_TREE_NUM_COLUMNS
};


enum bet_historical_data_columns {
    SPP_HISTORICAL_SELECT_COLUMN,
    SPP_HISTORICAL_DESC_COLUMN,
    SPP_HISTORICAL_PERIOD_COLUMN,
    SPP_HISTORICAL_BALANCE_COLUMN,
    SPP_HISTORICAL_AVERAGE_COLUMN,
    SPP_HISTORICAL_AMOUNT_COLUMN, /* average column without currency */
    SPP_HISTORICAL_BALANCE_COLOR,
    SPP_HISTORICAL_NUM_COLUMNS
};


static gchar* bet_duration_array[] = {
    N_("Month"),
    N_("Year"),
    NULL
};


/* the notebook of the bet */
static GtkWidget *bet_container = NULL;


/*
 * bet_create_balance_estimate_tab
 *
 * This function create the widget (notebook) which contains all the
 * balance estimate interface. This widget is added in the main window
 */
GtkWidget *bet_create_balance_estimate_tab ( void )
{
    GtkWidget* notebook;

    /* initialise structures */
    
    /* create a notebook for array and graph */
    notebook = gtk_notebook_new ( );
    gtk_widget_show ( notebook );
    bet_container = notebook;

    /****** Parameter page ******/
    bet_create_parameter_page ( notebook );

    /****** Estimation array page ******/
    bet_create_array_page ( notebook );

    /****** Estimation graph page ******/
    bet_create_graph_page ( notebook );

    /****** Estimation graph page ******/
    bet_create_historical_data_page ( notebook );

    return notebook;
}


/*
 * bet_update_balance_estimate_tab
 *
 * This function is called each time that "Balance estimate" is selected in the selection tree.
 */
void bet_update_balance_estimate_tab ( void )
{
    GtkWidget *tree_view;
    GtkTreeModel *tree_model;
    GtkTreeSelection* tree_selection;
    GtkTreeIter iter;
    GSList *tmp_list;
    gchar* previous_account_name = NULL;

    /* find the selected account */
    tree_view = g_object_get_data (G_OBJECT(bet_container), "bet_account_treeview");
    tree_model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    tree_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(tree_selection), &tree_model, &iter))
        gtk_tree_model_get (tree_model, &iter, 0, &previous_account_name, -1);

    /* fill the account list */
    gtk_tree_store_clear ( GTK_TREE_STORE (tree_model) );
    tmp_list = gsb_data_account_get_list_accounts ();
    while ( tmp_list )
    {
        gint i;
        gchar* account_name;

        i = gsb_data_account_get_no_account(tmp_list->data);
        tmp_list = tmp_list->next;
        if (gsb_data_account_get_closed_account(i))
            continue;
        account_name = gsb_data_account_get_name(i);
        gtk_tree_store_append (GTK_TREE_STORE(tree_model), &iter, NULL);
        gtk_tree_store_set(GTK_TREE_STORE(tree_model), &iter,
                   SPP_ACCOUNT_TREE_NAME_COLUMN, account_name, -1);
        /* select the previous selected account */
        if (previous_account_name && strcmp(account_name, previous_account_name) == 0)
            gtk_tree_selection_select_iter(GTK_TREE_SELECTION(tree_selection), &iter);
    }
    g_free(previous_account_name);

    /* if no row is selected, select the first line (if it exists) of the account list */
    if (!gtk_tree_selection_get_selected(GTK_TREE_SELECTION(tree_selection), &tree_model, &iter))
    {
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_model), &iter);
        gtk_tree_selection_select_iter(GTK_TREE_SELECTION(tree_selection), &iter);
    }

    /* update the estimate array graph and other data (if account list is not empty) */
    /* TODO clear the graph and the account list if no account is selected */
    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(tree_selection), &tree_model, &iter))
    {
        bet_estimate_refresh ( );
        bet_estimate_populate_historical_data ( );
    }
}


/*
 * bet_date_sort_function
 * This function is called by the Tree Model to sort
 * two lines by date.
 */
static gint bet_date_sort_function ( GtkTreeModel *model,
                        GtkTreeIter *itera,
                        GtkTreeIter *iterb,
                        gpointer user_data )
{
    g_assert(itera != NULL && iterb != NULL);

    /* get first date to compare */
    GValue date_value_a = {0,};
    gtk_tree_model_get_value(GTK_TREE_MODEL(model), itera,
			     SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
			     &date_value_a);
    GDate* datea = g_value_get_boxed(&date_value_a);
    g_assert(datea != NULL);
    g_assert(g_date_valid(datea));

    /* get second date to compare */
    GValue date_value_b = {0,};
    gtk_tree_model_get_value(GTK_TREE_MODEL(model), iterb,
			     SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
			     &date_value_b);
    GDate* dateb = g_value_get_boxed(&date_value_b);
    g_assert(dateb != NULL);
    g_assert(g_date_valid(dateb));

    gint result = g_date_compare(dateb, datea);

    g_value_unset(&date_value_b);
    g_value_unset(&date_value_a);

    return result;
}


/*
 * bet_update_average_column
 *
 * This function is called for each line of the array.
 * It calculates the balance column by adding the amount of the line
 * to the balance of the previous line.
 * It calculates the minimum and the maximum values of the balance column.
 */
static gboolean bet_update_average_column ( GtkTreeModel *model,
                        GtkTreePath *path,
                        GtkTreeIter *iter,
                        gpointer data )
{
    GtkWidget* dialog;
    GtkWidget *tree_view;
    GtkTreeModel *tree_model;
    GtkTreeIter iter_account;
    GtkTreeSelection* tree_selection;
    gchar *account_name;
    gchar *str_balance = NULL;
    gchar *tmp_str;
    gchar *color_str = NULL;
    gint selected_account;
    SBR *tmp_range = (SBR*) data;
    gsb_real amount;

    if ( tmp_range -> first_pass )
    {
        tmp_range -> first_pass = FALSE;
        return FALSE;
    }

    /* find the selected account */
    tree_view = g_object_get_data ( G_OBJECT ( bet_container ), "bet_account_treeview" );
    tree_model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );
    tree_selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( tree_view ) );
    if ( !gtk_tree_selection_get_selected ( GTK_TREE_SELECTION ( tree_selection ),
     &tree_model, &iter_account ) )
    {
        dialog = gtk_message_dialog_new ( GTK_WINDOW ( window ),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        "Please, select an account in the list !\n" );
        gtk_dialog_run ( GTK_DIALOG ( dialog ) );
        gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
        return FALSE;
    }
 
    account_name = NULL;
    gtk_tree_model_get ( tree_model, &iter_account, 0, &account_name, -1 );
    selected_account = gsb_data_account_get_no_account_by_name ( account_name );
    if ( selected_account == -1 )
        return FALSE;

    gtk_tree_model_get ( model, iter, SPP_ESTIMATE_TREE_AMOUNT_COLUMN, &tmp_str, -1 );

    amount = gsb_real_get_from_string ( tmp_str );

    tmp_range -> current_balance = gsb_real_add ( tmp_range -> current_balance, amount );
    str_balance = gsb_real_get_string_with_currency ( tmp_range -> current_balance, 
                                gsb_data_account_get_currency ( selected_account ), TRUE );

    if ( tmp_range->current_balance.mantissa < 0 )
        color_str = "red";
    else
        color_str = NULL;
    
    gtk_tree_store_set ( GTK_TREE_STORE ( model ),
                        iter,
                        SPP_ESTIMATE_TREE_BALANCE_COLUMN,
                        str_balance,
                        SPP_ESTIMATE_TREE_BALANCE_COLOR,
                        color_str,
                        -1 );
    g_free ( str_balance );

    if ( gsb_real_cmp ( tmp_range -> min_balance, tmp_range -> current_balance ) > 0 )
        tmp_range -> min_balance = tmp_range -> current_balance;
    if ( gsb_real_cmp ( tmp_range -> max_balance, tmp_range -> current_balance ) < 0 )
        tmp_range -> max_balance = tmp_range -> current_balance;

    return FALSE;
}


/*
 * bet_update_graph
 * This function is called for each line of the estimate array and it updates
 * the graph.
 */
static gboolean bet_update_graph ( GtkTreeModel *model,
                        GtkTreePath *path,
                        GtkTreeIter *iter,
                        gpointer data )
{
    /* get balance */
    gsb_real balance;
    gchar* balance_str = NULL;
    gtk_tree_model_get(model, iter, SPP_ESTIMATE_TREE_BALANCE_COLUMN, &balance_str, -1);
    balance = gsb_real_get_from_string(balance_str);
    g_free(balance_str);

    /* get date */
    GValue date_value = {0,};
    gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter,
			     SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
			     &date_value);
    /*
       GDate* date = g_value_get_boxed(&date_value);
       guint32 dt = g_date_get_julian(date);
       */
    g_value_unset(&date_value);
/* xxx ça semble pas fini ici... */
    return FALSE;
}


/*
 * bet_estimate_refresh
 * This function clears the estimate array and calculates new estimates.
 * It updates the estimate graph.
 * This function is called when the refresh button is pressed and when
 * the balance estimate tab is selected.
 */
static void bet_estimate_refresh ( void )
{
    /* find the selected account */
    GtkWidget *widget;
    GtkWidget *tree_view;
    GtkTreeIter iter;
    GtkTreeModel *tree_model;
    GtkTreeSelection *tree_selection;
    gchar *account_name = NULL;
    gchar *data;
    gchar *str_date_min;
    gchar *str_date_max;
    gchar *str_current_balance;
    gchar *title;
    gchar *tmp_str;
    gint selected_account;
    gint months;
    gint period;
    GDate *date_min;
    GDate *date_max;
    gsb_real current_balance;
    GSList* tmp_list;
    SBR *tmp_range;
    GValue date_value = {0, };

    tmp_range = initialise_struct_bet_range ( );
    tree_view = g_object_get_data ( G_OBJECT ( bet_container ), "bet_account_treeview" );
    tree_selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( tree_view ) );

    if ( !gtk_tree_selection_get_selected ( GTK_TREE_SELECTION ( tree_selection ),
     &tree_model, &iter ) )
    {
        /* no selection, select the first account
         * (no warning here because cause a conflict with the tree of navigation */
        gtk_notebook_set_current_page ( GTK_NOTEBOOK ( bet_container ), 0 );
        return;
    }

    gtk_tree_model_get ( tree_model, &iter, 0, &account_name, -1 );
    selected_account = gsb_data_account_get_no_account_by_name ( account_name );
    if ( selected_account == -1 )
        return;

    /* calculate date_min and date_max with user choice */
    data = g_object_get_data ( G_OBJECT ( bet_container ), "bet_months" );
    months = ( data ) ? GPOINTER_TO_INT ( data ): 1;
    data = g_object_get_data ( G_OBJECT ( bet_container ), "bet_deb_period" );
    period = ( data ) ? GPOINTER_TO_INT ( data ): 2;

    date_min = gdate_today ();
    if ( period == 1 )
        g_date_set_day ( date_min, 1 );

    data = g_object_get_data ( G_OBJECT ( bet_container ), "bet_end_period" );
    period = ( data ) ? GPOINTER_TO_INT ( data ): 2;

    date_max = gdate_today ();
    g_date_add_months (date_max, months );

    if ( period == 1 )
        date_max = gsb_date_get_last_day_of_month ( date_max );

    /* set the graph title and the array title */
    str_date_min = gsb_format_gdate ( date_min );
    g_value_init ( &date_value, G_TYPE_DATE );
    g_value_set_boxed ( &date_value, date_min );

    str_date_max = gsb_format_gdate (date_max);
    
    /* current balance may be in the future if there are transactions
     * in the future in the account. So we need to calculate the balance
     * of today */
    current_balance = gsb_data_account_calculate_current_day_balance (
                        selected_account, date_min );

    str_current_balance = gsb_real_get_string_with_currency ( current_balance,
                        gsb_data_account_get_currency ( selected_account ), TRUE );

    title = g_strdup_printf (
                        _("Balance estimate of the account \"%s\" from %s to %s"),
                        account_name, str_date_min, str_date_max );

    widget = GTK_WIDGET ( g_object_get_data ( G_OBJECT ( bet_container ), "bet_array_title") );
    gtk_label_set_label ( GTK_LABEL ( widget ), title );
    widget = GTK_WIDGET ( g_object_get_data ( G_OBJECT ( bet_container ), "bet_graph_title") );
    gtk_label_set_label ( GTK_LABEL ( widget ), title );
    g_free ( title );
    g_free ( account_name );

    /* clear tree view */
    tree_view = g_object_get_data ( G_OBJECT ( bet_container ), "bet_estimate_treeview" );
    tree_model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );
    gtk_tree_store_clear(GTK_TREE_STORE ( tree_model ) );

    /* current balance may be in the future if there are transactions
     * in the future in the account. So we need to calculate the balance
     * of today */
    current_balance = gsb_data_account_calculate_current_day_balance (
                        selected_account, date_min );

    str_current_balance = gsb_real_get_string_with_currency ( current_balance,
                        gsb_data_account_get_currency ( selected_account ), TRUE );

    tmp_str = g_strdup ( _("balance beginning of period") );
    gtk_tree_store_append ( GTK_TREE_STORE ( tree_model ), &iter, NULL );
    gtk_tree_store_set_value (GTK_TREE_STORE ( tree_model ), &iter,
                     SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
                     &date_value);
    gtk_tree_store_set(GTK_TREE_STORE(tree_model), &iter,
                   SPP_ESTIMATE_TREE_DATE_COLUMN, str_date_min,
                   SPP_ESTIMATE_TREE_DESC_COLUMN, tmp_str,
                   SPP_ESTIMATE_TREE_BALANCE_COLUMN, str_current_balance,
                   -1);

    g_value_unset ( &date_value );
    g_free(str_date_min);
    g_free(str_date_max);

    /* search transactions of the account which are in the future */
    tmp_list = gsb_data_transaction_get_transactions_list ( );

    while ( tmp_list )
    {
        gchar* str_amount;
        gchar* str_debit = NULL;
        gchar* str_credit = NULL;
        const gchar* str_description;
        gchar* str_date;
        gint transaction_number;
        gint transfer_number;
        gint account_number;
        gint transfer_account_number;
        const GDate *date;
        gsb_real amount;

        transaction_number = gsb_data_transaction_get_transaction_number ( tmp_list->data );
        tmp_list = tmp_list -> next;

        account_number =  gsb_data_transaction_get_account_number ( transaction_number );
        if ( account_number != selected_account )
            continue;

        date = gsb_data_transaction_get_date ( transaction_number );
        /* ignore transaction which are before date_min (today) */
        if ( g_date_compare ( date, date_min ) <= 0 )
            continue;
        /* ignore transaction which are after date_max */
        if ( g_date_compare (date, date_max ) > 0 )
            continue;

        /* ignore splitted transactions */
        if ( gsb_data_transaction_get_mother_transaction_number (
         transaction_number ) != 0 )
            continue;

        str_date = gsb_format_gdate ( date );
        g_value_init ( &date_value, G_TYPE_DATE );
        g_value_set_boxed ( &date_value, date );

        amount = gsb_data_transaction_get_amount ( transaction_number );
        str_amount = gsb_real_get_string ( amount );

        if (amount.mantissa < 0)
            str_debit = gsb_real_get_string_with_currency ( gsb_real_abs ( amount ),
                        gsb_data_transaction_get_currency_number ( transaction_number), TRUE );
        else
            str_credit = gsb_real_get_string_with_currency ( gsb_real_abs ( amount ),
                        gsb_data_transaction_get_currency_number (transaction_number), TRUE);

        transfer_number =
                        gsb_data_transaction_get_contra_transaction_number (
                        transaction_number );
        if ( transfer_number > 0 )
        {
            transfer_account_number = gsb_data_transaction_get_account_number (
                        transfer_number );
            str_description = g_strdup_printf ( _("Transfer between account: %s\n"
                        "and account: %s"),
                        gsb_data_account_get_name ( account_number ),
                        gsb_data_account_get_name ( transfer_account_number ) );
        }
        else
        {
            str_description = gsb_data_transaction_get_notes ( transaction_number );

            if (!str_description || !strlen (str_description))
                str_description = gsb_data_payee_get_name (
                            gsb_data_transaction_get_party_number ( transaction_number ), TRUE );
        }

        /* add a line in the estimate array */
        gtk_tree_store_append ( GTK_TREE_STORE ( tree_model ), &iter, NULL );
        gtk_tree_store_set_value (GTK_TREE_STORE ( tree_model ), &iter,
                     SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
                     &date_value);
        gtk_tree_store_set(GTK_TREE_STORE(tree_model), &iter,
                   SPP_ESTIMATE_TREE_DATE_COLUMN, str_date,
                   SPP_ESTIMATE_TREE_DESC_COLUMN, str_description,
                   SPP_ESTIMATE_TREE_DEBIT_COLUMN, str_debit,
                   SPP_ESTIMATE_TREE_CREDIT_COLUMN, str_credit,
                   SPP_ESTIMATE_TREE_AMOUNT_COLUMN, str_amount,
                   -1);

        g_value_unset ( &date_value );
        g_free ( str_date );
        g_free ( str_amount );
        if ( str_debit )
            g_free ( str_debit );
        if ( str_credit )
            g_free ( str_credit );
    }

    /* for each schedulded operation */
    tmp_list = gsb_data_scheduled_get_scheduled_list();

    while (tmp_list)
    {
        gchar *str_value;
        gchar *str_debit = NULL;
        gchar *str_credit = NULL;
        const gchar *str_description = NULL;
        gchar *str_date;
        gint scheduled_number;
        gint account_number;
        gint transfer_account_number;
        GDate *date;
        GValue date_value = {0, };
        gsb_real amount;

        scheduled_number = gsb_data_scheduled_get_scheduled_number ( tmp_list->data );

        tmp_list = tmp_list->next;

        /* ignore children scheduled operations */
        if (gsb_data_scheduled_get_mother_scheduled_number ( scheduled_number ) )
            continue;

        /* ignore scheduled operations of other account */
        account_number = gsb_data_scheduled_get_account_number ( scheduled_number );

        if ( gsb_data_scheduled_is_transfer ( scheduled_number ) )
        {
            transfer_account_number = gsb_data_scheduled_get_account_number_transfer (
                        scheduled_number );
            if ( transfer_account_number == selected_account )
            {
                str_description = g_strdup_printf ( _("Transfer between account: %s\n"
                        "and account: %s"),
                        gsb_data_account_get_name ( transfer_account_number ),
                        gsb_data_account_get_name ( account_number ) );
            
                amount = gsb_real_opposite ( gsb_data_scheduled_get_amount (
                        scheduled_number ) );
            }
            else if ( account_number == selected_account )
            {
                str_description = g_strdup_printf ( _("Transfer between account: %s\n"
                        "and account: %s"),
                        gsb_data_account_get_name ( account_number ),
                        gsb_data_account_get_name ( transfer_account_number ) );

                amount = gsb_data_scheduled_get_amount ( scheduled_number );
            }
            else
                continue;
        }
        else if ( account_number == selected_account )
        {
            str_description = gsb_data_scheduled_get_notes ( scheduled_number );

            if ( !str_description || !strlen ( str_description ) )
                str_description = gsb_data_payee_get_name (
                            gsb_data_scheduled_get_party_number ( scheduled_number ), TRUE );

            amount = gsb_data_scheduled_get_amount ( scheduled_number );
        }
        else
            continue;

        str_value = gsb_real_get_string ( amount );
        if (amount.mantissa < 0)
            str_debit = gsb_real_get_string_with_currency ( gsb_real_abs ( amount ),
                        gsb_data_scheduled_get_currency_number ( scheduled_number ), TRUE );
        else
            str_credit = gsb_real_get_string_with_currency ( gsb_real_abs ( amount ),
                        gsb_data_scheduled_get_currency_number ( scheduled_number ), TRUE );

        /* calculate each instance of the scheduled operation
         * in the range from date_min (today) to date_max */
        date = gsb_data_scheduled_get_date ( scheduled_number );

        while (date != NULL && g_date_valid ( date ) )
        {
            if ( g_date_compare ( date, date_max ) > 0 )
                break;
            if ( g_date_compare ( date, date_min ) < 0 )
            {
                date = gsb_scheduler_get_next_date ( scheduled_number, date );
                continue;
            }
            if ( g_date_valid ( date ) == FALSE )
                return;
            str_date = gsb_format_gdate ( date );

            g_value_init ( &date_value, G_TYPE_DATE );
            if ( date == NULL )
                return;
            g_value_set_boxed ( &date_value, date ); 

            /* add a line in the estimate array */
            gtk_tree_store_append (GTK_TREE_STORE(tree_model), &iter, NULL);
            gtk_tree_store_set_value(GTK_TREE_STORE(tree_model), &iter,
                         SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
                         &date_value);
            gtk_tree_store_set(GTK_TREE_STORE(tree_model), &iter,
                       SPP_ESTIMATE_TREE_DATE_COLUMN, str_date,
                       SPP_ESTIMATE_TREE_DESC_COLUMN, str_description,
                       SPP_ESTIMATE_TREE_DEBIT_COLUMN, str_debit,
                       SPP_ESTIMATE_TREE_CREDIT_COLUMN, str_credit,
                       SPP_ESTIMATE_TREE_AMOUNT_COLUMN, str_value,
                       -1);

            g_value_unset ( &date_value );
            g_free ( str_date );
            date = gsb_scheduler_get_next_date ( scheduled_number, date );
        }

        g_free ( str_value );
        if ( str_credit )
            g_free ( str_credit );
        if ( str_debit )
            g_free ( str_debit );
    }

    g_free ( date_min );
    g_free ( date_max );

    /* Calculate the balance column */
    tmp_range -> first_pass = TRUE;
    //~ tmp_range -> min_date = date_min;
    //~ tmp_range -> max_date = date_max;
    tmp_range -> min_balance = null_real;
    tmp_range -> max_balance = null_real;
    tmp_range -> current_balance = current_balance;

    gtk_tree_model_foreach ( GTK_TREE_MODEL ( tree_model ),
                        bet_update_average_column, tmp_range );

    /* update graph */
    widget = g_object_get_data ( G_OBJECT ( bet_container ), "bet_graph_curve" );
    gtk_curve_reset ( GTK_CURVE ( widget ) );
    gtk_tree_model_foreach ( GTK_TREE_MODEL ( tree_model ),
                        bet_update_graph, widget );
}


/*
 * bet_duration_period_clicked
 * This function is called when a radio button is called to change the inial period.
 * It copies the new durations from the data parameter (of the radio button) into
 * the bet_period property of the bet container
 */
static void bet_duration_period_clicked ( GtkWidget *togglebutton, gpointer data )
{
    const gchar *name;
    
    name = gtk_widget_get_name ( GTK_WIDGET ( togglebutton ) );
    if ( g_strcmp0 ( name, "button_1" ) == 0 )
        g_object_set_data ( G_OBJECT ( bet_container ), "bet_deb_period",
                        GINT_TO_POINTER ( 1 ) );
    else if ( g_strcmp0 ( name, "button_2" ) == 0 )
        g_object_set_data ( G_OBJECT ( bet_container ), "bet_deb_period",
                        GINT_TO_POINTER ( 2 ) );
    else if ( g_strcmp0 ( name, "button_3" ) == 0 )
        g_object_set_data ( G_OBJECT ( bet_container ), "bet_end_period",
                        GINT_TO_POINTER ( 1 ) );

    else if ( g_strcmp0 ( name, "button_4" ) == 0 )
        g_object_set_data ( G_OBJECT ( bet_container ), "bet_end_period",
                        GINT_TO_POINTER ( 2 ) );

    bet_estimate_refresh ( );
}


/*
 * bet_duration_button_clicked
 * This function is called when a radio button is called to change the estimate duration.
 * It copies the new durations from the data parameter (of the radio button) into
 * the bet_months property of the bet container
 */
static void bet_duration_button_clicked ( GtkWidget *togglebutton, GtkWidget *spin_button )
{
    gint months;
    const gchar *name;
    
    name = gtk_widget_get_name ( GTK_WIDGET ( togglebutton ) );
    months = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON ( spin_button ) );

    if ( g_strcmp0 ( name, "Year" ) == 0 )
    {
        gtk_spin_button_set_range ( GTK_SPIN_BUTTON ( spin_button ), 1.0, 20.0 );
        if ( months > 20 )
        {
            gtk_spin_button_set_value ( GTK_SPIN_BUTTON ( spin_button ), 20 );
            months = 240;
        }
        else
            months *= 12;
    }
    else
        gtk_spin_button_set_range ( GTK_SPIN_BUTTON ( spin_button ), 1.0, 240.0 );

    g_object_set_data ( G_OBJECT ( bet_container ), "bet_months",
                        GINT_TO_POINTER ( months ) );

    if ( etat.modification_fichier == 0 )
        modification_fichier ( TRUE );

    bet_estimate_refresh ( );
}


/**
 *
 *
 *
 *
 * */
void bet_historical_data_clicked ( GtkWidget *togglebutton, gpointer data )
{
    GtkTreeViewColumn *column;
    const gchar *name;
    gchar *title;

    name = gtk_widget_get_name ( GTK_WIDGET ( togglebutton ) );

    if ( g_strcmp0 ( name, "button_1" ) == 0 )
    {
        ptr_div = &gsb_data_transaction_get_category_number;
        ptr_sub_div = &gsb_data_transaction_get_sub_category_number;
        ptr_div_name = &gsb_data_category_get_name;
        title = g_strdup ( _("Category") );
    }
    else
    {
        ptr_div = &gsb_data_transaction_get_budgetary_number;
        ptr_sub_div = &gsb_data_transaction_get_sub_budgetary_number;
        ptr_div_name = gsb_data_budget_get_name;
        title = g_strdup ( _("Budgetary line") );
    }

    column = g_object_get_data ( G_OBJECT ( bet_container ),
                        "historical_column_source" );
    gtk_tree_view_column_set_title ( GTK_TREE_VIEW_COLUMN ( column ), title );

    if ( etat.modification_fichier == 0 )
        modification_fichier ( TRUE );

    bet_estimate_populate_historical_data ( );
}


/**
 *
 *
 *
 *
 * */
void bet_historical_fyear_clicked ( GtkWidget *combo, gpointer data )
{
    //~ GtkTreeViewColumn *column;
    //~ const gchar *name;
    //~ gchar *title;

    //~ name = gtk_widget_get_name ( GTK_WIDGET ( togglebutton ) );

    //~ if ( g_strcmp0 ( name, "button_1" ) == 0 )
    //~ {
        //~ ptr_div = &gsb_data_transaction_get_category_number;
        //~ ptr_sub_div = &gsb_data_transaction_get_sub_category_number;
        //~ ptr_div_name = &gsb_data_category_get_name;
        //~ title = g_strdup ( _("Category") );
    //~ }
    //~ else
    //~ {
        //~ ptr_div = &gsb_data_transaction_get_budgetary_number;
        //~ ptr_sub_div = &gsb_data_transaction_get_sub_budgetary_number;
        //~ ptr_div_name = gsb_data_budget_get_name;
        //~ title = g_strdup ( _("Budgetary line") );
    //~ }

    //~ column = g_object_get_data ( G_OBJECT ( bet_container ),
                        //~ "historical_column_source" );
    //~ gtk_tree_view_column_set_title ( GTK_TREE_VIEW_COLUMN ( column ), title );

    if ( etat.modification_fichier == 0 )
        modification_fichier ( TRUE );

    bet_estimate_populate_historical_data ( );
}


/*
 * bet_duration_button changed
 * This function is called when a spin button is changed.
 * It copies the new duration from the spin_button into the bet_months property of
 * the bet container
 */
gboolean bet_duration_number_changed ( GtkWidget *spin_button, GtkWidget *togglebutton )
{
    gint months = 0;
    gboolean toggled;

    months = gtk_spin_button_get_value_as_int ( GTK_SPIN_BUTTON ( spin_button ) );

    toggled = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( togglebutton ) );
    if ( toggled == 1 )
        months *= 12;

    g_object_set_data ( G_OBJECT ( bet_container ), "bet_months", GINT_TO_POINTER ( months ) );

    if ( etat.modification_fichier == 0 )
        modification_fichier ( TRUE );

    bet_estimate_refresh();

    return ( FALSE );
}


/*
 * bet_account_selection_changed
 * This function is called for each change in the selected line in the account list.
 * It refreshs balance estimation.
 */
void bet_account_selection_changed ( GtkTreeSelection *treeselection,
                        gpointer user_data )
{
    bet_estimate_refresh();
}


/**
 *
 *
 *
 *
 * */
void bet_create_parameter_page ( GtkWidget *notebook )
{
    GtkWidget *widget;
    GtkWidget *page;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *tree_view;

    widget = gtk_label_new ( _("Choice the prevision") );
    gtk_widget_show ( GTK_WIDGET ( widget ) );

    page = gtk_vbox_new ( FALSE, 5 );
    gtk_widget_show ( GTK_WIDGET ( page ) );
    gtk_notebook_append_page ( GTK_NOTEBOOK ( notebook ),
                        GTK_WIDGET ( page ), GTK_WIDGET ( widget ) );

    /* titre de la page */
    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_widget_show(GTK_WIDGET(hbox));
    gtk_box_pack_start ( GTK_BOX ( page ), hbox, FALSE, FALSE, 15 );
 
    widget = gtk_image_new_from_stock(
				      GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
    gtk_widget_show ( GTK_WIDGET ( widget ) );
    gtk_box_pack_start ( GTK_BOX ( hbox ), widget, FALSE, FALSE, 5 );

    widget = gtk_label_new ( NULL );
    gtk_label_set_markup ( GTK_LABEL ( widget ),
			 _("Please select an account and a duration\n"\
			   "and select the estimate array tab.") );
    gtk_widget_show ( GTK_WIDGET ( widget ) );
    gtk_box_pack_start ( GTK_BOX ( hbox ), widget, FALSE, FALSE, 5 );

    /* boite des paramètres */
    vbox = gtk_hbox_new ( FALSE, 5 );
    gtk_widget_show ( GTK_WIDGET ( vbox ) );
    gtk_box_pack_start ( GTK_BOX ( page ), vbox, FALSE, FALSE, 5 );

    /* create the account list */
    tree_view = bet_estimate_get_list_accounts ( vbox );
    g_object_set_data (G_OBJECT(notebook), "bet_account_treeview", tree_view);

    /* create duration selection */
    vbox = bet_estimate_get_duration_widget ( vbox );
}


/**
 *
 *
 *
 *
 * */
void bet_create_array_page ( GtkWidget *notebook )
{
    GtkWidget *widget;
    GtkWidget *vbox;
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    GtkTreeStore *tree_model;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    widget = gtk_label_new(_("Array"));
    gtk_widget_show(widget);
    vbox = gtk_vbox_new(FALSE, 5);
    gtk_widget_show(GTK_WIDGET(vbox));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(vbox), GTK_WIDGET(widget));

    /* create the title */
    widget = gtk_label_new("Estimate array");
    gtk_widget_show(GTK_WIDGET(widget));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(widget), FALSE, FALSE, 5);
    g_object_set_data (G_OBJECT(notebook), "bet_array_title", widget);

    /* create the estimate treeview */
    tree_view = gtk_tree_view_new();
    gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW (tree_view), TRUE);

    g_object_set_data (G_OBJECT(bet_container), "bet_estimate_treeview", tree_view);
    gtk_widget_show(tree_view);
    tree_model = gtk_tree_store_new ( SPP_ESTIMATE_TREE_NUM_COLUMNS,
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_DATE_COLUMN */
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_DESC_COLUMN */
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_DEBIT_COLUMN */
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_CREDIT_COLUMN */
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_BALANCE_COLUMN */
				    G_TYPE_DATE,   /* SPP_ESTIMATE_TREE_SORT_DATE_COLUMN */
				    G_TYPE_STRING, /* SPP_ESTIMATE_TREE_AMOUNT_COLUMN */
                    G_TYPE_STRING ); /*SPP_ESTIMATE_TREE_BALANCE_COLOR */
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(tree_model));
    g_object_unref (G_OBJECT(tree_model));

    /* sort by date */
    gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE(tree_model),
				      SPP_ESTIMATE_TREE_SORT_DATE_COLUMN,
				      (GtkTreeIterCompareFunc) bet_date_sort_function,
				      NULL, NULL );
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(tree_model),
					 SPP_ESTIMATE_TREE_SORT_DATE_COLUMN, GTK_SORT_DESCENDING);

    scrolled_window = gtk_scrolled_window_new ( NULL, NULL );
    gtk_widget_show(scrolled_window);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(scrolled_window), tree_view);
    gtk_widget_show(scrolled_window);
    gtk_box_pack_start(GTK_BOX(vbox),
		       GTK_WIDGET(scrolled_window), TRUE, TRUE, 5);

    /* Date column */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
						       _("Date"), cell,
						       "text", SPP_ESTIMATE_TREE_DATE_COLUMN,
						       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));
    gtk_tree_view_column_set_min_width(GTK_TREE_VIEW_COLUMN(column), 150);

    /* Description column */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
						       _("Description"), cell,
						       "text", SPP_ESTIMATE_TREE_DESC_COLUMN,
						       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));
    gtk_tree_view_column_set_min_width(column, 300);
    gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), TRUE);

    /* Debit column */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
						       _("Debit"), cell,
						       "text", SPP_ESTIMATE_TREE_DEBIT_COLUMN,
						       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));
    gtk_tree_view_column_set_min_width(column, 140);
    g_object_set(G_OBJECT(GTK_CELL_RENDERER(cell)), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment(column, 1);
    gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), FALSE);

    /* Credit column */
    cell = gtk_cell_renderer_text_new ();
    g_object_set(G_OBJECT(GTK_CELL_RENDERER(cell)), "xalign", 1.0, NULL );
    column = gtk_tree_view_column_new_with_attributes (
						       _("Credit"), cell,
						       "text", SPP_ESTIMATE_TREE_CREDIT_COLUMN,
						       NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));
    gtk_tree_view_column_set_min_width(column, 140);
    g_object_set(G_OBJECT(GTK_CELL_RENDERER(cell)), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment(column, 1);
    gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), FALSE);

    /* Balance column */
    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
					    _("Balance"), cell,
					    "text", SPP_ESTIMATE_TREE_BALANCE_COLUMN,
                        "foreground", SPP_ESTIMATE_TREE_BALANCE_COLOR,
					    NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));
    gtk_tree_view_column_set_min_width(column, 170);
    g_object_set(G_OBJECT(GTK_CELL_RENDERER(cell)), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment(column, 1);
    gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), FALSE);
}


/**
 *
 *
 *
 *
 * */
void bet_create_graph_page ( GtkWidget *notebook )
{
    GtkWidget *widget;
    GtkWidget *vbox;

    widget = gtk_label_new(_("Graph"));
    gtk_widget_show(widget);
    vbox = gtk_vbox_new(FALSE, 5);
    gtk_widget_show(GTK_WIDGET(vbox));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			     GTK_WIDGET(vbox), GTK_WIDGET(widget));

    /* create the title */
    widget = gtk_label_new("Estimation graph");
    gtk_widget_show(GTK_WIDGET(widget));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(widget), FALSE, FALSE, 5);
    g_object_set_data (G_OBJECT(notebook), "bet_graph_title", widget);

    widget = gtk_curve_new();
    gtk_widget_show(GTK_WIDGET(widget));
    gtk_box_pack_start(GTK_BOX(vbox),
		       GTK_WIDGET(widget), TRUE, TRUE, 5);
    g_object_set_data (G_OBJECT(notebook), "bet_graph_curve", widget);
}


/**
 *
 *
 *
 *
 * */
void bet_create_historical_data_page ( GtkWidget *notebook )
{
    GtkWidget *widget;
    GtkWidget *page;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *button_1, *button_2;
    GtkWidget *tree_view;

    widget = gtk_label_new(_("Historical data"));
    page = gtk_vbox_new ( FALSE, 5 );

    gtk_notebook_append_page ( GTK_NOTEBOOK ( notebook ),
                        GTK_WIDGET ( page ), GTK_WIDGET ( widget ) );

    /* titre de la page */
    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( page ), hbox, FALSE, FALSE, 15 );
 
    widget = gtk_image_new_from_stock(
                        GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start ( GTK_BOX ( hbox ), widget, FALSE, FALSE, 5 );

    vbox = gtk_vbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( hbox ), vbox, FALSE, FALSE, 5) ;

    widget = gtk_label_new ( NULL );
    gtk_label_set_markup ( GTK_LABEL ( widget ),
                        _("Please Select the data source") );
    gtk_box_pack_start ( GTK_BOX ( vbox ), widget, FALSE, FALSE, 5 );

    /* Choix des données sources */
    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( vbox ), hbox, FALSE, FALSE, 15 );

    button_1 = gtk_radio_button_new_with_label ( NULL,
                        _("Categories") );
    gtk_widget_set_name ( button_1, "button_1" );
    g_signal_connect (G_OBJECT ( button_1 ),
                        "released",
                        G_CALLBACK ( bet_historical_data_clicked ),
                        NULL );
    button_2 = gtk_radio_button_new_with_label_from_widget (
                        GTK_RADIO_BUTTON ( button_1 ),
                        _("Budgetary lines") );
    gtk_widget_set_name ( button_2, "button_2" );
    g_signal_connect (G_OBJECT ( button_2 ),
                        "released",
                        G_CALLBACK ( bet_historical_data_clicked ),
                        NULL );

    gtk_box_pack_start ( GTK_BOX ( hbox ), button_1, FALSE, FALSE, 5) ;
    gtk_box_pack_start ( GTK_BOX ( hbox ), button_2, FALSE, FALSE, 5) ;
    ptr_div = &gsb_data_transaction_get_category_number;
    ptr_sub_div = &gsb_data_transaction_get_sub_category_number;
    ptr_div_name = &gsb_data_category_get_name;

    /* vréation du sélecteur de périod */
    widget = gsb_fyear_make_combobox ( TRUE );
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( widget ),
					  SPACIFY(_("Choose the financial year or 12 months rolling") ) );
    g_signal_connect ( G_OBJECT ( widget ),
                        "changed",
                        G_CALLBACK (bet_historical_fyear_clicked),
                        NULL );
    g_object_set_data ( G_OBJECT ( notebook ), "bet_historical_fyear", widget );
    gtk_box_pack_start ( GTK_BOX ( hbox ), widget, FALSE, FALSE, 5) ;

    /* création de la liste des données */
    tree_view = bet_estimate_get_historical_data ( page );
    g_object_set_data ( G_OBJECT ( notebook ), "bet_historical_treeview", tree_view );
    bet_estimate_populate_historical_data ( );

    gtk_widget_show_all ( page );
}


/**
 *
 *
 *
 *
 * */
GtkWidget *bet_estimate_get_list_accounts ( GtkWidget *container )
{
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    GtkTreeStore *tree_model;
    GtkTreeSelection *tree_selection;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    tree_view = gtk_tree_view_new();

    tree_model = gtk_tree_store_new (SPP_ACCOUNT_TREE_NUM_COLUMNS, G_TYPE_STRING );
    gtk_tree_view_set_model ( GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL ( tree_model ) );
    g_object_unref ( G_OBJECT ( tree_model ) );

    tree_selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( tree_view ) );
    gtk_tree_selection_set_mode ( tree_selection, GTK_SELECTION_SINGLE );
    g_signal_connect ( G_OBJECT ( tree_selection ),
                        "changed",
                        G_CALLBACK (bet_account_selection_changed),
                        NULL );

    scrolled_window = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_widget_set_size_request ( scrolled_window, -1, 250 );
    gtk_container_add (GTK_CONTAINER(scrolled_window), tree_view);
    gtk_widget_show(scrolled_window);
    gtk_box_pack_start ( GTK_BOX ( container ), scrolled_window, TRUE, TRUE, 15 );

    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
                        _("Account"), cell,
                        "text", SPP_ACCOUNT_TREE_NAME_COLUMN,
                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
				GTK_TREE_VIEW_COLUMN(column));

    gtk_widget_show_all ( scrolled_window );

    return tree_view;
}


/**
 *
 *
 *
 *
 * */
GtkWidget *bet_estimate_get_duration_widget ( GtkWidget *container )
{
    GtkWidget* main_vbox;
    GtkWidget *label;
    GtkWidget *spin_button, *button_1, *button_2, *button_3, *button_4;
    GtkWidget *widget;
    GtkWidget *hbox;
    GtkWidget *previous = NULL;
    GtkSizeGroup *size_group;
    gint iduration;

    size_group = gtk_size_group_new ( GTK_SIZE_GROUP_HORIZONTAL );

    main_vbox = gtk_vbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( container ), main_vbox, FALSE, FALSE, 5) ;

    label = gtk_label_new ( _("Calculation of period") );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), label, FALSE, FALSE, 5) ;

    label = gtk_label_new ( _("Beginning of period") );
    gtk_misc_set_alignment ( GTK_MISC ( label ), 0, 0.5);
    gtk_label_set_justify ( GTK_LABEL ( label ), GTK_JUSTIFY_LEFT );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), label, FALSE, FALSE, 5) ;

    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), hbox, FALSE, FALSE, 5) ;

    button_1 = gtk_radio_button_new_with_label ( NULL,
                        _("1st day of month") );
    gtk_widget_set_name ( button_1, "button_1" );
    gtk_size_group_add_widget ( GTK_SIZE_GROUP ( size_group ), button_1 );
    
    g_signal_connect (G_OBJECT ( button_1 ),
                        "released",
                        G_CALLBACK ( bet_duration_period_clicked ),
                        NULL );
    g_object_set_data ( G_OBJECT ( bet_container ), "bet_deb_period",
                        GINT_TO_POINTER ( 1 ) );
    button_2 = gtk_radio_button_new_with_label_from_widget (
                        GTK_RADIO_BUTTON ( button_1 ),
                        _("date today") );
    gtk_widget_set_name ( button_2, "button_2" );

    g_signal_connect (G_OBJECT ( button_2 ),
                        "released",
                        G_CALLBACK ( bet_duration_period_clicked ),
                        NULL );

    gtk_box_pack_start ( GTK_BOX ( hbox ), button_1, FALSE, FALSE, 5) ;
    gtk_box_pack_start ( GTK_BOX ( hbox ), button_2, FALSE, FALSE, 5) ;

    label = gtk_label_new ( _("End of period") );
    gtk_misc_set_alignment ( GTK_MISC ( label ), 0, 0.5);
    gtk_label_set_justify ( GTK_LABEL ( label ), GTK_JUSTIFY_LEFT );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), label, FALSE, FALSE, 5) ;

    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), hbox, FALSE, FALSE, 5) ;

    button_3 = gtk_radio_button_new_with_label ( NULL,
                        _("last day of the month") );
    gtk_widget_set_name ( button_3, "button_3" );
    gtk_size_group_add_widget ( GTK_SIZE_GROUP ( size_group ), button_3 );
    
    g_signal_connect (G_OBJECT ( button_3 ),
                        "released",
                        G_CALLBACK ( bet_duration_period_clicked ),
                        NULL );
    g_object_set_data ( G_OBJECT ( bet_container ), "bet_end_period",
                        GINT_TO_POINTER ( 1 ) );
    button_4 = gtk_radio_button_new_with_label_from_widget (
                        GTK_RADIO_BUTTON ( button_3 ),
                        _("From date to date") );
    gtk_widget_set_name ( button_4, "button_4" );
    g_signal_connect (G_OBJECT ( button_4 ),
                        "released",
                        G_CALLBACK ( bet_duration_period_clicked ),
                        NULL );

    gtk_box_pack_start ( GTK_BOX ( hbox ), button_3, FALSE, FALSE, 5) ;
    gtk_box_pack_start ( GTK_BOX ( hbox ), button_4, FALSE, FALSE, 5) ;

    
    /* partie mensuelle */
    label = gtk_label_new ( _("Duration estimation") );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), label, FALSE, FALSE, 5) ;

    hbox = gtk_hbox_new ( FALSE, 5 );
    gtk_box_pack_start ( GTK_BOX ( main_vbox ), hbox, FALSE, FALSE, 5) ;

    spin_button = gtk_spin_button_new_with_range ( 1.0, 240.0, 1.0);

    for (iduration = 0; bet_duration_array[iduration] != NULL; iduration++)
    {
        if (previous == NULL)
        {
            widget = gtk_radio_button_new_with_label ( NULL,
                        _(bet_duration_array[iduration]) );
            previous = widget;
        } 
        else 
        {
            widget = gtk_radio_button_new_with_label_from_widget (
                        GTK_RADIO_BUTTON ( previous ),
                        _(bet_duration_array[iduration]) );
        }
        gtk_widget_set_name ( widget, bet_duration_array[iduration] );
        gtk_box_pack_start ( GTK_BOX ( hbox ), widget, FALSE, FALSE, 5 );
        g_signal_connect (G_OBJECT ( widget ),
                        "released",
                        G_CALLBACK ( bet_duration_button_clicked ),
                        spin_button );
    }

    g_signal_connect ( G_OBJECT ( spin_button ),
                        "value-changed",
                        G_CALLBACK ( bet_duration_number_changed ),
                        widget );
    gtk_box_pack_start ( GTK_BOX ( hbox ), spin_button, FALSE, FALSE, 0 );
    
    gtk_widget_show_all ( main_vbox );

    return main_vbox;
}


/**
 *
 *
 *
 *
 * */
GtkWidget *bet_estimate_get_historical_data ( GtkWidget *container )
{
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    GtkTreeStore *tree_model;
    GtkTreeSelection *tree_selection;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    tree_view = gtk_tree_view_new ( );
    gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW (tree_view), TRUE);

    tree_model = gtk_tree_store_new ( SPP_HISTORICAL_NUM_COLUMNS,
                        G_TYPE_BOOLEAN,G_TYPE_STRING,
                        G_TYPE_STRING, G_TYPE_STRING,
                        G_TYPE_STRING, G_TYPE_STRING,
                        G_TYPE_STRING );
    gtk_tree_view_set_model ( GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL ( tree_model ) );
    g_object_unref ( G_OBJECT ( tree_model ) );

    gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE ( tree_model ),
					    SPP_HISTORICAL_DESC_COLUMN, GTK_SORT_ASCENDING );

    tree_selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( tree_view ) );
    gtk_tree_selection_set_mode ( tree_selection, GTK_SELECTION_SINGLE );
    //~ g_signal_connect ( G_OBJECT ( tree_selection ),
                        //~ "changed",
                        //~ G_CALLBACK (bet_account_selection_changed),
                        //~ NULL );

    scrolled_window = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scrolled_window ),
                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_widget_set_size_request ( scrolled_window, -1, 250 );
    gtk_container_add ( GTK_CONTAINER ( scrolled_window ), tree_view );
    gtk_box_pack_start ( GTK_BOX ( container ), scrolled_window, TRUE, TRUE, 15 );

    /* create columns */
    /* select column */ 
    cell = gtk_cell_renderer_toggle_new ( );
    gtk_cell_renderer_toggle_set_radio ( GTK_CELL_RENDERER_TOGGLE ( cell ),
                        FALSE );
    g_object_set (cell, "xalign", 0.5, NULL);

    column = gtk_tree_view_column_new_with_attributes ( _("Select"),
                        cell,
                        "active", SPP_HISTORICAL_SELECT_COLUMN,
                        NULL);
    gtk_tree_view_column_set_alignment ( column, 0.5 );
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ), column );

    /* name of the div sous-div column*/
    cell = gtk_cell_renderer_text_new ( );
    column = gtk_tree_view_column_new_with_attributes (
                        _("Category"), cell,
                        "text", SPP_HISTORICAL_DESC_COLUMN,
                        NULL);
    g_object_set_data ( G_OBJECT ( bet_container ), "historical_column_source",
                        column );
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ),
                        GTK_TREE_VIEW_COLUMN ( column ) );
    gtk_tree_view_column_set_min_width ( column, 300 );
    gtk_tree_view_column_set_expand ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );

    /* period analysis column */
    cell = gtk_cell_renderer_text_new ( );
    column = gtk_tree_view_column_new_with_attributes (
                        _("Period"), cell,
                        "text", SPP_HISTORICAL_PERIOD_COLUMN,
                        NULL);
    gtk_tree_view_column_set_alignment ( column, 0.5 );
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ),
                        GTK_TREE_VIEW_COLUMN ( column ) );
    gtk_tree_view_column_set_min_width ( column, 120 );
    gtk_tree_view_column_set_expand ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );

    /* amount column */
    cell = gtk_cell_renderer_text_new ( );
    column = gtk_tree_view_column_new_with_attributes (
                        _("Amount"), cell,
                        "text", SPP_HISTORICAL_BALANCE_COLUMN,
                        "foreground", SPP_HISTORICAL_BALANCE_COLOR,
                        NULL);
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ),
                        GTK_TREE_VIEW_COLUMN ( column ) );
    gtk_tree_view_column_set_min_width ( column, 100 );
    g_object_set ( G_OBJECT ( GTK_CELL_RENDERER ( cell ) ), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment ( column, 1 );
    gtk_tree_view_column_set_clickable ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );
    gtk_tree_view_column_set_expand ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );

    /* average column */
    cell = gtk_cell_renderer_text_new ( );
    column = gtk_tree_view_column_new_with_attributes (
                        _("Average"), cell,
                        "text", SPP_HISTORICAL_AVERAGE_COLUMN,
                        "foreground", SPP_HISTORICAL_BALANCE_COLOR,
                        NULL);
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ),
                        GTK_TREE_VIEW_COLUMN ( column ) );
    gtk_tree_view_column_set_min_width ( column, 100 );
    g_object_set ( G_OBJECT ( GTK_CELL_RENDERER ( cell ) ), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment ( column, 1 );
    gtk_tree_view_column_set_clickable ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );
    gtk_tree_view_column_set_expand ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );

    /* amount retained column */
    cell = gtk_cell_renderer_text_new ( );
    g_object_set (cell, "editable", TRUE, NULL);
    //~ g_signal_connect ( cell,
                        //~ "edited",
                        //~ G_CALLBACK (gsb_import_associations_cell_edited),
                        //~ vbox_main );
    column = gtk_tree_view_column_new_with_attributes (
                        _("Amount retained"), cell,
                        "text", SPP_HISTORICAL_AVERAGE_COLUMN,
                        "foreground", SPP_HISTORICAL_BALANCE_COLOR,
                        NULL);
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( tree_view ),
                        GTK_TREE_VIEW_COLUMN ( column ) );
    gtk_tree_view_column_set_min_width ( column, 100 );
    g_object_set ( G_OBJECT ( GTK_CELL_RENDERER ( cell ) ), "xalign", 1.0, NULL );
    gtk_tree_view_column_set_alignment ( column, 1 );
    gtk_tree_view_column_set_clickable ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );
    gtk_tree_view_column_set_expand ( GTK_TREE_VIEW_COLUMN ( column ), TRUE );

    gtk_widget_show_all ( scrolled_window );

    return tree_view;
}


/**
 *
 *
 *
 *
 * */
void bet_estimate_populate_historical_data ( void )
{
    GtkWidget *tree_view;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *tree_selection;
    gchar *account_name = NULL;
    gint selected_account;
    GDate *date_min;
    GDate *date_max;
    GSList* tmp_list;
    GHashTable  *list_div;

    /* récuperation du n° de compte à utiliser */
    tree_view = g_object_get_data ( G_OBJECT ( bet_container ), "bet_account_treeview" );
    tree_selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( tree_view ) );

    if ( !gtk_tree_selection_get_selected ( GTK_TREE_SELECTION ( tree_selection ),
     &model, &iter ) )
    {
        /* no selection, select the first account
         * (no warning here because cause a conflict with the tree of navigation */
        gtk_notebook_set_current_page ( GTK_NOTEBOOK ( bet_container ), 0 );
        return;
    }

    gtk_tree_model_get ( model, &iter, 0, &account_name, -1 );
    selected_account = gsb_data_account_get_no_account_by_name ( account_name );
    if ( selected_account == -1 )
        return;

    tree_view = g_object_get_data (G_OBJECT ( bet_container ), "bet_historical_treeview" );
    model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );
    gtk_tree_store_clear ( GTK_TREE_STORE ( model ) );

    /* calculate date_min and date_max */
    date_min = gdate_today ( );
    g_date_subtract_years ( date_min, 1 );
    date_max = gdate_today ( );
    g_date_subtract_days ( date_max, 1 );

    list_div = g_hash_table_new_full ( g_int_hash,
                        g_int_equal,
                        NULL,
                        (GDestroyNotify) free_struct_historical );

    /* search transactions of the account  */
    tmp_list = gsb_data_transaction_get_complete_transactions_list ( );

    while ( tmp_list )
    {
        gint transaction_number;
        gint account_number;
        const GDate *date;

        transaction_number = gsb_data_transaction_get_transaction_number (
                        tmp_list->data );
        tmp_list = tmp_list -> next;

        account_number =  gsb_data_transaction_get_account_number (
                        transaction_number );
        if ( account_number != selected_account )
            continue;

        date = gsb_data_transaction_get_date ( transaction_number );
        /* ignore transaction which are before date_min (today) */
        if ( g_date_compare ( date, date_min ) <= 0 )
            continue;
        /* ignore transaction which are after date_max */
        if ( g_date_compare (date, date_max ) > 0 )
            continue;

        /* ignore splitted transactions */
        if ( gsb_data_transaction_get_mother_transaction_number (
         transaction_number ) != 0 )
            continue;

        bet_estimate_populate_div ( transaction_number, list_div );
    }
    bet_estimate_affiche_div ( list_div, tree_view );

    g_hash_table_remove_all ( list_div );
}


/**
 *
 *
 *
 *
 * */
gboolean bet_estimate_populate_div ( gint transaction_number,
                        GHashTable  *list_div )
{
    gint div = 0;
    gint sub_div = 0;
    SH *sh = NULL;

    div = ptr_div ( transaction_number );
    if ( div > 0 )
        sub_div = ptr_sub_div ( transaction_number );
    else
        return FALSE;

    if ( g_hash_table_lookup_extended ( list_div, &div, NULL, (gpointer) &sh ) )
        bet_estimate_update_div ( sh, transaction_number, sub_div );
    else
    {
        sh = initialise_struct_historical ( );
        sh -> div = div;
        sh -> account_nb = gsb_data_transaction_get_account_number ( transaction_number );
        bet_estimate_update_div ( sh, transaction_number, sub_div );
        g_hash_table_insert ( list_div, &div, sh );
    }

    return FALSE;
}


/**
 *
 *
 *
 *
 * */
gboolean bet_estimate_update_div ( SH *sh, gint transaction_number, gint sub_div )
{
    SBR *sbr = ( SBR*) sh -> sbr;
    gsb_real amount;
    SH *tmp_sh = NULL;

    amount = gsb_data_transaction_get_amount ( transaction_number );
    sbr-> current_balance = gsb_real_add ( sbr -> current_balance, amount );

    if ( sub_div == -1 )
        return FALSE;

    if ( g_hash_table_lookup_extended ( sh -> list_sub_div, &sub_div,
     NULL, (gpointer) &tmp_sh ) )
        bet_estimate_update_div ( tmp_sh, transaction_number, -1 );
    else
    {
        tmp_sh = initialise_struct_historical ( );
        tmp_sh -> div = sub_div;
        tmp_sh -> account_nb = gsb_data_transaction_get_account_number ( transaction_number );
        bet_estimate_update_div ( tmp_sh, transaction_number, -1 );
        g_hash_table_insert ( sh -> list_sub_div, &sub_div, tmp_sh );
    }

    return FALSE;
}


/**
 *
 *
 *
 *
 * */
gboolean bet_estimate_affiche_div ( GHashTable  *list_div, GtkWidget *tree_view )
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );

    g_hash_table_foreach ( list_div, bet_estimate_populate_div_model, model );

    return FALSE;
}


/**
 *
 *
 *
 *
 * */
void bet_estimate_populate_div_model ( gpointer key,
                        gpointer value,
                        gpointer user_data)
{
    SH *sh = ( SH* ) value;
    SBR *sbr = sh -> sbr;
    GtkTreeModel *model = ( GtkTreeModel * ) user_data;
    GtkTreeIter parent;
    GHashTableIter iter;
    gpointer sub_key, sub_value;
    gint div;
    gchar *div_name = NULL;
    gchar *str_balance;
    gchar *str_average;
    gsb_real period = { 12, 0 };

    div = sh -> div;
    div_name = ptr_div_name ( div, 0, FALSE );

    str_balance = gsb_real_get_string_with_currency ( sbr -> current_balance, 
                        gsb_data_account_get_currency ( sh -> account_nb ), TRUE );
    str_average = gsb_real_get_string_with_currency (
                        gsb_real_div ( sbr -> current_balance, period ),
                        gsb_data_account_get_currency ( sh -> account_nb ), TRUE );

    gtk_tree_store_append ( GTK_TREE_STORE ( model ), &parent, NULL);
    gtk_tree_store_set ( GTK_TREE_STORE ( model ),
                        &parent,
                        SPP_HISTORICAL_DESC_COLUMN, div_name,
                        SPP_HISTORICAL_PERIOD_COLUMN, "12 months rolling",
                        SPP_HISTORICAL_BALANCE_COLUMN, str_balance,
                        SPP_HISTORICAL_AVERAGE_COLUMN, str_average,
                        -1);
    g_free ( div_name );
    g_free ( str_balance );
    g_free ( str_average );

    if ( g_hash_table_size ( sh -> list_sub_div ) == 1 )
        return;

    g_hash_table_iter_init ( &iter, sh -> list_sub_div );
    while ( g_hash_table_iter_next ( &iter, &sub_key, &sub_value ) ) 
    {
        SH *sub_sh = ( SH* ) sub_value;
        SBR *sub_sbr = sub_sh -> sbr;
        GtkTreeIter fils;
        gchar **tab_str = NULL;

        div_name = ptr_div_name ( div, sub_sh -> div, FALSE );
        if ( g_utf8_strrchr ( div_name, -1, ':' ) )
        {
	        tab_str = g_strsplit ( div_name, ":", 2 );
            if (tab_str[1])
                div_name = g_strdup ( g_strstrip ( tab_str[1] ) );
            if ( tab_str )
                g_strfreev ( tab_str );
        }
            
        str_balance = gsb_real_get_string_with_currency ( sub_sbr -> current_balance, 
                        gsb_data_account_get_currency ( sh -> account_nb ), TRUE );
        str_average = gsb_real_get_string_with_currency (
                        gsb_real_div ( sub_sbr -> current_balance, period ),
                        gsb_data_account_get_currency ( sh -> account_nb ), TRUE );

        gtk_tree_store_append ( GTK_TREE_STORE ( model ), &fils, &parent );
        gtk_tree_store_set ( GTK_TREE_STORE ( model ),
                        &fils,
                        SPP_HISTORICAL_DESC_COLUMN, div_name,
                        SPP_HISTORICAL_PERIOD_COLUMN, "12 months rolling",
                        SPP_HISTORICAL_BALANCE_COLUMN, str_balance,
                        SPP_HISTORICAL_AVERAGE_COLUMN, str_average,
                        -1);

        g_free ( div_name );
    }
}
/**
 *
 *
 *
 *
 * */
SBR *initialise_struct_bet_range ( void )
{
	SBR	*sbr;
	
	sbr = g_malloc ( sizeof ( SBR ) );
    sbr -> first_pass = TRUE;
    sbr -> min_date = NULL;
    sbr -> max_date = NULL;
    sbr -> min_balance = null_real;
    sbr -> max_balance = null_real;
    sbr -> current_balance = null_real;

	return sbr;
}


/**
 *
 *
 *
 *
 * */
void free_struct_bet_range ( SBR *sbr )
{
    if ( sbr-> min_date )
        g_date_free ( sbr-> min_date );
    if ( sbr-> max_date )
        g_date_free ( sbr-> max_date );

    g_free ( sbr );
}


SH *initialise_struct_historical ( void )
{
	SH	*sh;

	sh = g_malloc ( sizeof ( SH ) );
    sh -> sbr = initialise_struct_bet_range ( );
    sh -> list_sub_div = g_hash_table_new_full ( g_int_hash,
                        g_int_equal,
                        NULL,
                        (GDestroyNotify) free_struct_historical );
	return sh;
}


/**
 *
 *
 *
 *
 * */
void free_struct_historical (SH *sh)
{

    if ( sh -> sbr )
        free_struct_bet_range ( sh -> sbr );
    if ( sh -> list_sub_div )
        g_hash_table_remove_all ( sh -> list_sub_div );
    g_free ( sh );
}


/**
 *
 *
 *
 *
 * */
#endif /* ENABLE_BALANCE_ESTIMATE */
