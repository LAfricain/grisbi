/* ************************************************************************** */
/* work with the struct of payee                                              */
/*                                                                            */
/*                                                                            */
/*     Copyright (C)	2000-2005 C�dric Auger (cedric@grisbi.org)	      */
/* 			http://www.grisbi.org				      */
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

/**
 * \file gsb_payee_data.c
 * work with the payee structure, no GUI here
 */


#include "include.h"

/*START_INCLUDE*/
#include "gsb_payee_data.h"
#include "gsb_transaction_data.h"
#include "tiers_onglet.h"
#include "traitement_variables.h"
#include "include.h"
#include "structures.h"
/*END_INCLUDE*/


/*START_STATIC*/
static gint gsb_payee_get_pointer_from_name_in_glist ( struct_payee *payee,
						gchar *name );
static gint gsb_payee_max_number ( void );
static void gsb_payee_reset_counters ( void );
/*END_STATIC*/

/*START_EXTERN*/
extern     gchar * buffer ;
extern GSList *liste_struct_etats;
/*END_EXTERN*/

/** contains a g_slist of struct_payee */
static GSList *payee_list;

/** a pointer to the last payee used (to increase the speed) */
static struct_payee *payee_buffer;

/** a pointer to a "blank" payee structure, used in the list of payee
 * to group the transactions without payee */
static struct_payee *without_payee;


/**
 * set the payees global variables to NULL, usually when we init all the global variables
 *
 * \param none
 *
 * \return FALSE
 * */
gboolean gsb_payee_init_variables ( void )
{
    payee_list = NULL;
    payee_buffer = NULL;

    /* create the blank payee */

    without_payee = calloc ( 1, sizeof ( struct_payee ));
    without_payee -> payee_name = _("No payee");



    return FALSE;
}


/**
 * find and return the structure of the payee asked
 *
 * \param no_payee number of payee
 *
 * \return the adr of the struct of the payee (NULL if doesn't exit)
 * */
gpointer gsb_payee_get_structure ( gint no_payee )
{
    GSList *tmp;

    if (!no_payee)
	return NULL;

    /* before checking all the payees, we check the buffer */

    if ( payee_buffer
	 &&
	 payee_buffer -> payee_number == no_payee )
	return payee_buffer;

    tmp = payee_list;
    
    while ( tmp )
    {
	struct_payee *payee;

	payee = tmp -> data;

	if ( payee -> payee_number == no_payee )
	{
	    payee_buffer = payee;
	    return payee;
	}

	tmp = tmp -> next;
    }
    return NULL;
}


/**
 * give the g_slist of payees structure
 * usefull when want to check all payees
 *
 * \param none
 *
 * \return the g_slist of payees structure
 * */
GSList *gsb_payee_get_payees_list ( void )
{
    return payee_list;
}

/**
 * return the number of the payees given in param
 *
 * \param payee_ptr a pointer to the struct of the payee
 *
 * \return the number of the payee, 0 if problem
 * */
gint gsb_payee_get_no_payee ( gpointer payee_ptr )
{
    struct_payee *payee;
    
    if ( !payee_ptr )
	return 0;
    
    payee = payee_ptr;
    payee_buffer = payee;
    return payee -> payee_number;
}


/** find and return the last number of payee
 * \param none
 * \return last number of payee
 * */
gint gsb_payee_max_number ( void )
{
    GSList *tmp;
    gint number_tmp = 0;

    tmp = payee_list;
    
    while ( tmp )
    {
	struct_payee *payee;

	payee = tmp -> data;

	if ( payee -> payee_number > number_tmp )
	    number_tmp = payee -> payee_number;

	tmp = tmp -> next;
    }
    return number_tmp;
}


/**
 * create a new payee, give him a number, append it to the list
 * and return the number
 * update combofix and mark file as modified
 *
 * \param name the name of the payee (can be freed after, it's a copy) or NULL
 *
 * \return the number of the new payee
 * */
gint gsb_payee_new ( gchar *name )
{
    struct_payee *payee;

    payee = calloc ( 1,
		     sizeof ( struct_payee ));
    payee -> payee_number = gsb_payee_max_number () + 1;
    if (name)
	payee -> payee_name = g_strdup (name);

    payee_list = g_slist_append ( payee_list,
				  payee );

    mise_a_jour_combofix_tiers ();
    modification_fichier(TRUE);

    return payee -> payee_number;
}

/**
 * remove a payee
 * set all the payees of transaction which are this one to 0
 * update combofix and mark file as modified
 *
 * \param no_payee the payee we want to remove
 *
 * \return TRUE ok
 * */
gboolean gsb_payee_remove ( gint no_payee )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return FALSE;
    
    payee_list = g_slist_remove ( payee_list,
				  payee );
    
    mise_a_jour_combofix_tiers ();
    modification_fichier(TRUE);
    return TRUE;
}


/**
 * set a new number for the payee
 * normally used only while loading the file because
 * the number are given automaticly
 *
 * \param no_payee the number of the payee
 * \param new_no_payee the new number of the payee
 *
 * \return the new number or 0 if the payee doen't exist
 * */
gint gsb_payee_set_new_number ( gint no_payee,
				gint new_no_payee )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return 0;

    payee -> payee_number = new_no_payee;
    return new_no_payee;
}


/**
 * return the number of the payee wich has the name in param
 * create it if necessary
 *
 * \param name the name of the payee
 * \param create TRUE if we want to create it if it doen't exist
 *
 * \return the number of the payee or 0 if problem
 * */
gint gsb_payee_get_number_by_name ( gchar *name,
				    gboolean create )
{
    GSList *list_tmp;
    gint payee_number = 0;

    list_tmp = g_slist_find_custom ( payee_list,
				     name,
				     (GCompareFunc) gsb_payee_get_pointer_from_name_in_glist );
    
    if ( list_tmp )
    {
	struct_payee *payee;
	
	payee = list_tmp -> data;
	payee_number = payee -> payee_number;
    }
    else
    {
	if (create)
	    payee_number = gsb_payee_new (name);
    }

    return payee_number;
}


/**
 * used with g_slist_find_custom to find a payee in the g_list
 * by his name
 *
 * \param payee the struct of the current payee checked
 * \param name the name we are looking for
 *
 * \return 0 if it's the same name
 * */
gint gsb_payee_get_pointer_from_name_in_glist ( struct_payee *payee,
						gchar *name )
{
    return ( g_strcasecmp ( payee -> payee_name,
			    name ));
}

/**
 * return the name of the payee
 *
 * \param no_payee the number of the payee
 * \param can_return_null if problem, return NULL if TRUE or "No payee" if FALSE
 *
 * \return the name of the payee or NULL/No payee if problem
 * */
gchar *gsb_payee_get_name ( gint no_payee,
			    gboolean can_return_null)
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
    {
	if (can_return_null)
	    return NULL;
	else
	    return (  g_strdup (_("No payee defined")));
    }

    return payee -> payee_name;
}


/**
 * set the name of the payee
 * the value is dupplicate in memory
 *
 * \param no_payee the number of the payee
 * \param name the name of the payee
 *
 * \return TRUE if ok or FALSE if problem
 * */
gboolean gsb_payee_set_name ( gint no_payee,
			      const gchar *name )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return FALSE;

    /* we free the last name */

    if ( payee -> payee_name )
	free (payee -> payee_name);
    
    /* and copy the new one */
    payee -> payee_name = g_strdup (name);
    return TRUE;
}

/**
 * return a g_slist of names of all the payees
 * it's not a copy of the gchar...
 *
 * \param none
 *
 * \return a g_slist of gchar *
 * */
GSList *gsb_payee_get_name_list ( void )
{
    GSList *return_list;
    GSList *tmp_list;

    return_list = NULL;
    tmp_list= payee_list;

    while ( tmp_list )
    {
	struct_payee *payee;

	payee = tmp_list -> data;

	return_list = g_slist_append ( return_list,
				       payee -> payee_name );
	tmp_list = tmp_list -> next;
    }
    return return_list;
}

/**
 * return a g_slist of names of all the payees and
 * the name of the reports which have to be with the payees
 * it's not a copy of the gchar...
 *
 * \param none
 *
 * \return a g_slist of gchar *
 * */
GSList *gsb_payee_get_name_and_report_list ( void )
{
    GSList *return_list;
    GSList *tmp_list;
    GSList *pointer;


    /* for the transactions list, it's a complex type of list, so a g_slist
     * which contains some g_slist of names of payees, one of the 2 g_slist
     * is the selected reports names */

    tmp_list= gsb_payee_get_name_list ();
    return_list = NULL;
    return_list = g_slist_append ( return_list,
				   tmp_list );

    /* we append the selected reports */

    tmp_list = NULL;
    pointer = liste_struct_etats;

    while ( pointer )
    {
	struct struct_etat *etat;

	etat = pointer -> data;

	if ( etat -> inclure_dans_tiers )
	{
	    if ( tmp_list )
		tmp_list = g_slist_append ( tmp_list,
					    g_strconcat ( "\t",
							  g_strdup ( etat -> nom_etat ),
							  NULL ));
	    else
	    {
		tmp_list = g_slist_append ( tmp_list,
					    _("Report"));
		tmp_list = g_slist_append ( tmp_list,
					    g_strconcat ( "\t",
							  g_strdup ( etat -> nom_etat ),
							  NULL ));
	    }
	}
	pointer = pointer -> next;
    }

    return_list = g_slist_append ( return_list,
				   tmp_list );

    return return_list;
}


/**
 * return the description of the payee
 *
 * \param no_payee the number of the payee
 *
 * \return the description of the payee or NULL if problem
 * */
gchar *gsb_payee_get_description ( gint no_payee )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return NULL;

    return payee -> payee_description;
}


/**
 * set the description of the payee
 * the value is dupplicate in memory
 *
 * \param no_payee the number of the payee
 * \param description the description of the payee
 *
 * \return TRUE if ok or FALSE if problem
 * */
gboolean gsb_payee_set_description ( gint no_payee,
				     const gchar *description )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return FALSE;

    /* we free the last name */

    if ( payee -> payee_description )
	free (payee -> payee_description);
    
    /* and copy the new one */
    payee -> payee_description = g_strdup (description);
    return TRUE;
}




/**
 * return nb_transactions of the payee
 *
 * \param no_payee the number of the payee
 *
 * \return nb_transactions of the payee or 0 if problem
 * */
gint gsb_payee_get_nb_transactions ( gint no_payee )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return 0;

    return payee -> payee_nb_transactions;
}



/**
 * return balance of the payee
 *
 * \param no_payee the number of the payee
 *
 * \return balance of the payee or 0 if problem
 * */
gdouble gsb_payee_get_balance ( gint no_payee )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( no_payee );

    if (!payee)
	return 0;

    return payee -> payee_balance;
}


/**
 * reset the counters of the payees
 *
 * \param 
 *
 * \return 
 * */
void gsb_payee_reset_counters ( void )
{
    GSList *list_tmp;

    list_tmp = payee_list;

    while ( list_tmp )
    {
	struct_payee *payee;

	payee = list_tmp -> data;
	payee -> payee_balance = 0.0;
	payee -> payee_nb_transactions = 0;

	list_tmp = list_tmp -> next;
    }
    
    /* reset the blank payee counters */

    without_payee -> payee_balance = 0.0;
    without_payee -> payee_nb_transactions = 0;
}

/**
 * update the counters of the payees
 *
 * \param
 *
 * \return
 * */
void gsb_payee_update_counters ( void )
{
    GSList *list_tmp_transactions;

    gsb_payee_reset_counters ();

    list_tmp_transactions = gsb_transaction_data_get_transactions_list ();

    while ( list_tmp_transactions )
    {
	gint transaction_number_tmp;
	transaction_number_tmp = gsb_transaction_data_get_transaction_number (list_tmp_transactions -> data);
	
	gsb_payee_add_transaction_to_payee ( transaction_number_tmp );

	list_tmp_transactions = list_tmp_transactions -> next;
    }
}


/**
 * add the given transaction to its payee in the counters
 * if the transaction has no payee, add it to the blank payee
 *
 * \param transaction_number the transaction we want to work with
 *
 * \return
 * */
void gsb_payee_add_transaction_to_payee ( gint transaction_number )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( gsb_transaction_data_get_party_number (transaction_number));

    /* if no payee in that transaction, and it's neither a breakdown, neither a transfer,
     * we work with without_payee */

    if (!payee
	&&
	!gsb_transaction_data_get_breakdown_of_transaction (transaction_number)
	&& 
	!gsb_transaction_data_get_transaction_number_transfer (transaction_number))
	payee = without_payee;

	payee -> payee_nb_transactions ++;
	payee -> payee_balance += gsb_transaction_data_get_adjusted_amount (transaction_number);
}


/**
 * remove the given transaction to its payee in the counters
 * if the transaction has no payee, remove it to the blank payee
 *
 * \param transaction_number the transaction we want to work with
 *
 * \return
 * */
void gsb_payee_remove_transaction_from_payee ( gint transaction_number )
{
    struct_payee *payee;

    payee = gsb_payee_get_structure ( gsb_transaction_data_get_party_number (transaction_number));

    /* if no payee in that transaction, and it's neither a breakdown, neither a transfer,
     * we work with without_payee */

    if (!payee
	&&
	!gsb_transaction_data_get_breakdown_of_transaction (transaction_number)
	&& 
	!gsb_transaction_data_get_transaction_number_transfer (transaction_number))
	payee = without_payee;

	payee -> payee_nb_transactions --;
	payee -> payee_balance -= gsb_transaction_data_get_adjusted_amount (transaction_number);

	if ( !payee -> payee_nb_transactions ) /* Cope with float errors */
	    payee -> payee_balance = 0.0;
}


/**
 * return a pointer to the blank struct payee
 *
 * \param
 *
 * \return a pointer to the without_payee struct
 * */
gpointer gsb_payee_get_without_payee ( void )
{
    return (gpointer) without_payee;
}
