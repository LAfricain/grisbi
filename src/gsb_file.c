/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2003-2008 Benjamin Drieu (bdrieu@april.org)	                      */
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "gsb_file.h"
#include "accueil.h"
#include "affichage_liste.h"
#include "dialog.h"
#include "fenetre_principale.h"
#include "grisbi_prefs.h"
#include "gsb_account_property.h"
#include "gsb_assistant_account.h"
#include "gsb_assistant_file.h"
#include "gsb_data_account.h"
#include "gsb_data_archive_store.h"
#include "gsb_dirs.h"
#include "gsb_file_load.h"
#include "gsb_file_save.h"
#include "gsb_file_util.h"
#include "gsb_real.h"
#include "gsb_status.h"
#include "gsb_transactions_list.h"
#include "menu.h"
#include "navigation.h"
#include "parametres.h"
#include "structures.h"
#include "traitement_variables.h"
#include "transaction_list.h"
#include "utils.h"
#include "utils_files.h"
#include "utils_file_selection.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/

/*START_STATIC*/
static void gsb_file_append_name_to_opened_list ( gchar *path_fichier );
static gboolean gsb_file_automatic_backup ( gpointer null );
static gchar *gsb_file_dialog_ask_name ( void );
static gint gsb_file_dialog_save ( gchar *filename );
static gboolean gsb_file_save_backup ( void );
static gboolean gsb_file_save_file ( gint origine );
static void gsb_file_save_remove_old_file ( gchar *filename );
/*END_STATIC*/


/**
 * keep the last path used in grisbi, save in the configuration at the end */
static gchar *last_path_used;

/** the timeout used to save a backup every x minutes */
gint id_timeout = 0;

/*START_EXTERN*/
extern gchar *copy_old_filename;
extern GtkWidget *table_etat;
extern GtkWidget *tree_view_vbox;
/*END_EXTERN*/


/**
 * Called by menu file -> new,
 * close the last file and open a new one
 * in fact just an assistant launcher, but need to check if the previous file is closed
 *
 * \param none
 *
 * \return FALSE
 * */
gboolean gsb_file_new ( void )
{
    /* continue only if closing the file is ok */
    if ( !gsb_file_close () )
        return FALSE;

    /* set up all the default variables */
/* 	init_variables ();  */

    gsb_assistant_file_run (FALSE, FALSE);

    return FALSE;
}


/**
 * come here to finish to create the new file
 * at this time, categories and currencies must created
 * this part will just launch the assistant to create a new account
 * a finish the initialisation to lauch grisbi
 *
 * \param
 *
 * \return FALSE
 * */
gboolean gsb_file_new_finish ( void )
{
    /* create the first account */
    if (! gsb_assistant_account_run ())
    {
/*         init_variables ();  */
        return FALSE;
    }

    /* init the gui */
    gsb_gui_new_gui ();

    mise_a_jour_accueil ( TRUE );
    gsb_gui_navigation_set_selection ( GSB_HOME_PAGE, -1, NULL );

    gsb_file_set_modified ( TRUE );
    return FALSE;
}


/**
 * called by file -> open
 * open a new file
 *
 * \param
 *
 * \return FALSE
 * */
gboolean gsb_file_open_menu ( void )
{
    GtkWidget *selection_fichier;
    GtkFileFilter * filter;

    selection_fichier = gtk_file_chooser_dialog_new ( _("Open an accounts file"),
                        GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ),
                        GTK_FILE_CHOOSER_ACTION_OPEN,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                        NULL);

    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( selection_fichier ), gsb_file_get_last_path () );
    gtk_window_set_position ( GTK_WINDOW ( selection_fichier ), GTK_WIN_POS_CENTER_ON_PARENT );

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, _("Grisbi files (*.gsb)") );
    gtk_file_filter_add_pattern ( filter, "*.gsb" );
    gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER ( selection_fichier ), filter );
    gtk_file_chooser_set_filter ( GTK_FILE_CHOOSER ( selection_fichier ), filter );

    filter = gtk_file_filter_new ();
    gtk_file_filter_set_name ( filter, _("All files") );
    gtk_file_filter_add_pattern ( filter, "*" );
    gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER ( selection_fichier ), filter );

    switch ( gtk_dialog_run ( GTK_DIALOG ( selection_fichier) ) )
    {
    case GTK_RESPONSE_OK:
        if ( gsb_file_close () )
        {
            gchar *nom_fichier_comptes;

            gtk_widget_hide ( selection_fichier );
            nom_fichier_comptes = file_selection_get_filename ( GTK_FILE_CHOOSER ( selection_fichier ) );
            gsb_file_update_last_path ( file_selection_get_last_directory (
                            GTK_FILE_CHOOSER ( selection_fichier ),
                            TRUE ) );
            gsb_file_open_file ( nom_fichier_comptes );

            g_free ( nom_fichier_comptes );
        }
        break;
    default:
        break;
    }

    gtk_widget_destroy ( selection_fichier );

    return FALSE;
}


/**
 * update the variable last_path_used with the path given in param
 *
 * \param last_path
 *
 * \return
 * */
void gsb_file_update_last_path ( const gchar *last_path )
{
    if ( last_path && strlen ( last_path ) )
    {
        if ( last_path_used )
            g_free ( last_path_used );

        last_path_used = my_strdup ( last_path );
    }
}

/**
 * the last_path_used is local variable,
 * that return the content of that variable
 *
 * \param
 *
 * \return a const gchar, the last path used in grisbi
 * */
const gchar *gsb_file_get_last_path ( void )
{
    return last_path_used;
}

/**
 * get the backup path
 *
 * \param conf
 *
 * \return a const gchar with the backup path
 * */
const gchar *gsb_file_get_backup_path ( GrisbiAppConf *conf )
{
    GrisbiAppConf *tmp_conf;

    if ( conf == NULL )
        tmp_conf = grisbi_app_get_conf ();
    else
        tmp_conf = conf;

    return tmp_conf->backup_path;
}


/**
 * set the backup path
 *
 * \param bakcup path
 * \param GrisbiAppConf
 *
 * \return
 * */
void gsb_file_set_backup_path ( const gchar *path,
                        GrisbiAppConf *conf )
{
    if ( conf->backup_path )
        g_free ( conf->backup_path );

    if ( path == NULL || strlen ( path ) == 0 )
        conf->backup_path = my_strdup ( gsb_dirs_get_user_config_dir ( ) );
    else
        conf->backup_path = my_strdup ( path );

    if ( !g_file_test ( conf->backup_path, G_FILE_TEST_EXISTS ) )
    {
#ifdef _MSC_VER
        int mode = 0;
#else
        int mode = S_IRUSR | S_IWUSR | S_IXUSR;
#endif /*_MSC_VER */

        g_mkdir_with_parents ( conf->backup_path, mode );
    }
}


/**
 * called by file -> last files -> click on the name
 *
 * \param item
 * \param file_number_ptr the number of file in a pointer format
 *
 * \return FALSE
 * */
gboolean gsb_file_open_direct_menu ( GtkMenuItem *item,
                        gint *file_number_ptr )
{
    GrisbiApp *app;
    GrisbiAppConf *conf;
    gint file_number;

    /* continue only if can close the current file */
    if ( !gsb_file_close ( ) )
        return FALSE;

    app = grisbi_app_get_default ( );
    conf = grisbi_app_get_conf ( );

    file_number = GPOINTER_TO_INT ( file_number_ptr );
    gsb_file_open_file ( conf->tab_noms_derniers_fichiers_ouverts[file_number] );

    return FALSE;
}


/**
 * open a new grisbi file, don't check anything about another opened file that must
 * have been done before
 *
 * \para filename the name of the file
 *
 * \return TRUE ok, FALSE problem
 * */
gboolean gsb_file_open_file ( gchar *filename )
{
    GtkWidget *tree_view;
    GSList *list_tmp;
    GrisbiAppConf *conf;
    GrisbiWindowRun *run;

    devel_debug (filename);

    if ( !filename || strlen ( filename ) == 0 )
        return FALSE;

    gsb_status_wait ( TRUE );
    gsb_status_message ( _("Loading accounts") );

    /* try to load the file */
    if ( !g_file_test ( filename, G_FILE_TEST_EXISTS ) )
    {
        utils_files_display_dialog_error ( G_FILE_TEST_EXISTS, filename, g_strerror ( errno ) );
        gsb_file_remove_name_from_opened_list (filename);

        return FALSE;
    }

    /* check here if it's not a regular file */
    if ( !g_file_test ( filename, G_FILE_TEST_IS_REGULAR ))
    {
        utils_files_display_dialog_error ( G_FILE_TEST_IS_REGULAR, filename, NULL );
        gsb_file_remove_name_from_opened_list ( filename );

        return ( FALSE );
    }

    conf = grisbi_app_get_conf ( );

    /* on fixe le nom de fichier associé à la fenêtre active */
    grisbi_app_set_active_filename ( filename );

    /* on positionne la variable run->is_loading */
    run = grisbi_window_get_struct_run ( NULL );
    run->is_loading = TRUE;

    /* on initialise la variable etat */
    grisbi_window_init_struct_etat ();

    /* le fichier existe et est un fichier "normal" on l'ouvre */
    if ( gsb_file_load_open_file ( filename ) )
    {
        /* the file has been opened succesfully we make a backup if necessary */
        if ( conf->sauvegarde_demarrage )
        {
            gsb_file_save_backup ();
            gsb_file_set_modified ( FALSE );
        }
    }
    else
    {
        /* Loading failed. */
        gsb_status_message ( _("Failed to load accounts") );

        if ( conf->sauvegarde_demarrage || conf->make_backup || conf->make_backup_every_minutes )
        {
            utils_files_display_dialog_error ( GSB_FAILED_LOAD_ACCOUNTS, filename, NULL );
            gsb_status_stop_wait ( TRUE );
            run->is_loading = FALSE;

            return FALSE;
        }
        else
        {
            if ( gsb_file_get_backup_path ( conf ) )
                utils_files_display_dialog_error ( GSB_FAILED_LOAD_WITH_BACKUP, filename, NULL );
            else
                utils_files_display_dialog_error ( GSB_FAILED_LOAD_WITHOUT_BACKUP, filename, NULL );
            gsb_status_stop_wait ( TRUE );
            run->is_loading = FALSE;

            return FALSE;
        }
    }

    /* ok, here the file or backup is loaded */
    gsb_status_message ( _("Checking schedulers") );

    /* the the name in the last opened files */
    gsb_file_append_name_to_opened_list ( filename );

    /* create all the gui */
    gsb_gui_new_gui ();

    /* Create transaction list. */
    tree_view = grisbi_window_get_widget_by_name ( "treeview_transactions" );
    if ( !gsb_transactions_list_make_gui_list ( tree_view ) )
        return FALSE;

    /* check the amounts of all the accounts */
    gsb_status_message ( _("Checking amounts") );
    list_tmp = gsb_data_account_get_list_accounts ( );

    while ( list_tmp )
    {
        gint account_number;
        volatile gint value;

        account_number = gsb_data_account_get_no_account ( list_tmp -> data );

        /* set the minimum balances to be shown or not */
        value = gsb_real_cmp ( gsb_data_account_get_current_balance (account_number),
                              gsb_data_account_get_mini_balance_authorized (account_number) ) == -1;
        gsb_data_account_set_mini_balance_authorized_message ( account_number, value);
        value = gsb_real_cmp ( gsb_data_account_get_current_balance (account_number),
                              gsb_data_account_get_mini_balance_wanted (account_number) ) == -1;
        gsb_data_account_set_mini_balance_wanted_message ( account_number, value);
        list_tmp = list_tmp -> next;
    }

    /* set Grisbi title */
    grisbi_window_set_active_title ( -1 );

    /* update the main page */
    mise_a_jour_accueil (TRUE);

    /* for now, the flag for modification of the file is ok, but the menu couldn't be set
     * as sensitive/unsensitive so do it now */
    gsb_file_set_modified ( gsb_file_get_modified ( ) );

    gsb_status_message ( _("Done") );
    gsb_status_stop_wait ( TRUE );
    run->is_loading = FALSE;

    /* go to the home page */
    gsb_gui_navigation_set_selection ( GSB_HOME_PAGE, -1, NULL );

    /* set the focus to the selection tree at left */
    gtk_widget_grab_focus ( gsb_gui_navigation_get_tree_view ( ) );

    /* refresh prefs if necessary */
    grisbi_prefs_refresh_preferences ( TRUE );

    /* return */
    return TRUE;
}


/**
 * Perform the "Save" feature in menu
 *
 * \return TRUE on success.  FALSE otherwise.
 */
gboolean gsb_file_save ( void )
{
    return gsb_file_save_file ( 0 );
}


/**
 * Perform the "Save as" feature in menu
 *
 * \return TRUE on success.  FALSE otherwise.
 */
gboolean gsb_file_save_as ( void )
{
    return gsb_file_save_file ( -2 );
}


/**
 * save the file
 *
 * \param origine 0 from gsb_file_save (menu), -1 from gsb_file_close, -2 from gsb_file_save_as
 *
 * \return TRUE if ok, FALSE if problem
 * */
gboolean gsb_file_save_file ( gint origine )
{
    gint etat_force, result;
    gchar *nouveau_nom_enregistrement;
    gchar *nom_fichier_comptes = NULL;
    GrisbiAppConf *conf;
    GrisbiWindowEtat *etat;

    devel_debug_int (origine);

    conf = grisbi_app_get_conf ( );
    etat = grisbi_window_get_struct_etat ();

    etat_force = 0;

    if ( ( !gsb_file_get_modified ( ) && origine != -2 ) ||
     !gsb_data_account_get_accounts_amount () )
    {
        notice_debug ( "nothing done in gsb_file_save_file" );
        return ( TRUE );
    }

    /* si le fichier de comptes n'a pas de nom ou si on enregistre sous un nouveau nom */
    /*     c'est ici */
    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );
    if ( !nom_fichier_comptes || origine == -2 )
    {
        nouveau_nom_enregistrement = gsb_file_dialog_ask_name ();
        if ( origine == -2 )
            g_free ( nom_fichier_comptes );
    }
    else
        nouveau_nom_enregistrement = nom_fichier_comptes;

    if ( !nouveau_nom_enregistrement )
        return FALSE;

    /*     on vérifie que le fichier n'est pas locké */
    if ( etat->fichier_deja_ouvert
     &&
     !conf->force_enregistrement
     &&
     origine != -2 )
    {
        gchar *tmpstr1;
        gchar *tmpstr2;

        tmpstr1 = g_strdup_printf( _("Grisbi was unable to save this file because it is locked. "
                        "Please save it with another name or activate the \"%s\" option in preferences."),
                        ("Force saving of locked files" ) );
        tmpstr2 = g_strdup_printf( _("Can not save file \"%s\""), nouveau_nom_enregistrement );
        dialogue_error_hint ( tmpstr1, tmpstr2 );

        g_free ( tmpstr1 );
        g_free ( tmpstr2 );

        return ( FALSE );
    }

        /* Si le fichier est un d'une version précédente de grisbi on demande si on l'efface */
    if ( copy_old_filename && strlen ( copy_old_filename ) > 0 )
    {
        gsb_file_save_remove_old_file ( copy_old_filename );
        g_free ( copy_old_filename );
        copy_old_filename = NULL;
    }

    /* make backup before saving if asked */
    if ( conf->make_backup )
        gsb_file_save_backup();

    /*   on a maintenant un nom de fichier */
    /*     et on sait qu'on peut sauvegarder */
    gsb_status_message ( _("Saving file") );

    result = gsb_file_save_save_file ( nouveau_nom_enregistrement,
                        conf->compress_file,
                        FALSE );

    if ( result )
    {
        /* saving was right, so unlock the last name */
        gsb_file_util_modify_lock ( FALSE );

        grisbi_app_set_active_filename ( nouveau_nom_enregistrement );

        /* and lock the new name */
        gsb_file_util_modify_lock ( TRUE );

        /* update variables */
        etat->fichier_deja_ouvert = 0;
            gsb_file_set_modified ( FALSE );
        grisbi_window_set_active_title ( gsb_gui_navigation_get_current_account ( ) );
        gsb_file_append_name_to_opened_list ( nouveau_nom_enregistrement );

        g_free ( nouveau_nom_enregistrement );
    }

    gsb_status_message ( _("Done") );

    return ( result );
}


/**
 * save a backup of the file
 *
 * \param
 *
 * \return TRUE ok, FALSE problem
 * */
gboolean gsb_file_save_backup ( void )
{
    gboolean retour;
    gchar *filename;
    struct tm *day_time;
    time_t temps;
    gchar *name;
    gchar *nom_fichier_comptes;
    GrisbiAppConf *conf;

    conf = grisbi_app_get_conf ( );

    if (!gsb_file_get_backup_path ( conf ) || !gsb_file_get_modified () )
        return FALSE;

    gsb_status_message ( _("Saving backup") );

    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );
    name = g_path_get_basename ( nom_fichier_comptes );
    if ( g_str_has_suffix ( name, ".gsb" ) )
    {
        gchar **tab_str;

        tab_str = g_strsplit ( name, ".gsb", 0 );
        if ( tab_str[0] && strlen ( tab_str[0] ) )
        {
            g_free ( name );
            name = g_strdup ( tab_str[0] );
        }
        g_strfreev ( tab_str );
    }
    /* create a filename for the backup :
     * filename_yyyymmddTmmhhss.gsb */
    if ( conf->make_bakup_single_file )
    {
        filename =  g_strdup_printf ( "%s%s%s_backup.gsb",
                            gsb_file_get_backup_path ( conf ),
                            G_DIR_SEPARATOR_S,
                            name );
    }
    else
    {
        time ( &temps );
        day_time = localtime (&temps);
        filename =  g_strdup_printf ( "%s%s%s_%d%02d%02dT%02d%02d%02d.gsb",
                            gsb_file_get_backup_path ( conf ),
                            G_DIR_SEPARATOR_S,
                            name,
                            day_time -> tm_year + 1900,
                            day_time -> tm_mon + 1,
                            day_time -> tm_mday,
                            day_time -> tm_hour,
                            day_time -> tm_min,
                            day_time -> tm_sec );
    }

    retour = gsb_file_save_save_file ( filename, conf->compress_backup, FALSE );

    g_free (filename);
    g_free (name);
    g_free ( nom_fichier_comptes );

    gsb_status_message ( _("Done") );

    return ( retour );
}


/**
 * called when the user select the backup every x minutes
 *
 * \param checkbutton
 * \param null
 *
 * \return FALSE
 * */
gboolean gsb_file_automatic_backup_start ( GtkWidget *checkbutton,
                        gpointer null )
{
/*     devel_debug_int (conf->make_backup_every_minutes);  */

    /* if there is already a timeout, we remove it */
    if (id_timeout)
    {
	g_source_remove (id_timeout);
	id_timeout = 0;
    }

    /* launch the timeout only if active and if there is some minutes */
/*     if (conf->make_backup_every_minutes
 * 	&&
 * 	conf->make_backup_nb_minutes )
 * 	id_timeout = g_timeout_add_seconds ( conf->make_backup_nb_minutes * 60,
 * 					     (GSourceFunc) (gsb_file_automatic_backup),
 * 					     NULL );
 */

    /* return */
    return FALSE;
}

/**
 * called when the user change the interval of automatic backup
 *
 * \param spinbutton
 * \param null
 *
 * \return FALSE
 * */
gboolean gsb_file_automatic_backup_change_time ( GtkWidget *spinbutton,
                        gpointer null )
{
    GrisbiAppConf *conf;

    conf = grisbi_app_get_conf ( );
    devel_debug_int (conf->make_backup_nb_minutes);

    /* if there is already a timeout, we stop it */
    if (id_timeout)
    {
        g_source_remove (id_timeout);
        id_timeout = 0;
    }

    /* set a new timeout only if there is an interval */
    if (conf->make_backup_nb_minutes)
        id_timeout = g_timeout_add_seconds ( conf->make_backup_nb_minutes * 60,
                        (GSourceFunc) (gsb_file_automatic_backup),
                        NULL );

    /* return */
    return FALSE;
}


/**
 * called every x minutes defined by user
 * to save a backup of the file
 *
 * \param null
 *
 * \return TRUE to continue the timeout, FALSE to stop the timeout
 * */
gboolean gsb_file_automatic_backup ( gpointer null )
{
    GrisbiAppConf *conf;

    devel_debug (NULL);
    conf = grisbi_app_get_conf ( );

    if ( !conf->make_backup_every_minutes )
        /* stop the timeout */
        return FALSE;

    /* we save only if there is a nb of minutes, but don't stop the timer if not */
    if ( conf->make_backup_nb_minutes )
        gsb_file_save_backup ();

    return TRUE;
}


/**
 * propose to save the file if changed
 * if the current file is not changed, return GTK_RESPONSE_NO directly, without dialog
 *
 * \param
 *
 * \return GTK_RESPONSE_OK to save, GTK_RESPONSE_NO not to save, other to cancel
 * */
static gint gsb_file_dialog_save ( gchar *filename )
{
    gchar * hint;
    gchar* time_elapsed;
    time_t now = time ( NULL );
    gint result;
    GtkWidget *dialog;
    gint difference;
    gchar* message;
    gchar* tmpstr1;
    gchar* tmpstr2;
    GrisbiAppConf *conf;
    GrisbiWindowEtat *etat;
    GrisbiWindowRun *run;

    /*     si le fichier n'est pas modifié on renvoie qu'on ne veut pas enregistrer */
    if ( !gsb_file_get_modified ( ) )
        return GTK_RESPONSE_NO;

    conf = grisbi_app_get_conf ( );
    etat = grisbi_window_get_struct_etat ();

    if ( conf->sauvegarde_auto
     &&
     ( !etat->fichier_deja_ouvert || conf->force_enregistrement )
     &&
     filename )
    {
        return GTK_RESPONSE_OK;
    }

    /*     si le fichier était déjà locké et que force enregistrement n'est pas mis, */
    /*     on prévient ici */

    dialog = gtk_message_dialog_new ( GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_WARNING,
                        GTK_BUTTONS_NONE,
                        NULL );
    if ( etat->fichier_deja_ouvert && !conf->force_enregistrement )
    {
        hint = g_strdup ( _("Save locked files?") );
        message = g_strdup_printf ( _("The document '%s' is locked but modified. "
                        "If you want to save it, you must cancel and save it with "
                        "another name or activate the \"%s\" option in setup."),
                        ( filename ? g_path_get_basename ( filename ) : _("unnamed") ),
                        _("Force saving of locked files") );

        gtk_dialog_add_buttons ( GTK_DIALOG ( dialog ),
                        _("Close without saving"), GTK_RESPONSE_NO,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        NULL );
        gtk_dialog_set_default_response ( GTK_DIALOG ( dialog ), GTK_RESPONSE_CANCEL );
    }
    else
    {
        hint = g_strdup_printf ( _("Save changes to document '%s' before closing?"),
                        ( filename ? g_path_get_basename ( filename ) : _("unnamed") ) );

        message = g_strdup("");
        gtk_dialog_add_buttons ( GTK_DIALOG(dialog),
                        _("Close without saving"), GTK_RESPONSE_NO,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                        NULL );
        gtk_dialog_set_default_response ( GTK_DIALOG ( dialog ), GTK_RESPONSE_OK );
    }

    run = grisbi_window_get_struct_run ( NULL );
    difference = (gint) difftime ( now, run->file_modification );

    if ( difference >= 120 )
    {
        time_elapsed = g_strdup_printf ( _( "%d minutes and %d seconds" ),
                        difference / 60, difference % 60 );
    }
    else if ( difference >= 60 )
    {
        time_elapsed = g_strdup_printf ( _( "1 minute and %d seconds" ), difference % 60 );
    }
    else
    {
        time_elapsed = g_strdup_printf ( _( "%d seconds" ), difference );
    }

    tmpstr1 = message;
    tmpstr2 = g_strdup_printf ( _("If you close without saving, all of your changes "
                        "since %s will be discarded."),
                        time_elapsed );

    message = g_strconcat ( tmpstr1, tmpstr2 , NULL );
    g_free ( tmpstr1 );
    g_free ( tmpstr2 );
    g_free ( time_elapsed );

    gtk_label_set_markup ( GTK_LABEL ( GTK_MESSAGE_DIALOG ( dialog ) ), make_hint ( hint, message ) );

    g_free ( message );
    g_free ( hint );

    result = gtk_dialog_run ( GTK_DIALOG ( dialog ) );

    gtk_widget_destroy ( dialog );

    return result;
}



/**
 * ask the name of the file to save it
 *
 * \param
 *
 * \return a newly allocated string containing the new name
 * */
static gchar *gsb_file_dialog_ask_name ( void )
{
    gchar *new_name;
    gchar *nom_fichier_comptes;
    GtkWidget *dialog;
    gint result;

    dialog = gtk_file_chooser_dialog_new ( _("Name the accounts file"),
                        GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ),
                        GTK_FILE_CHOOSER_ACTION_SAVE,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                        NULL);

    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), gsb_file_get_last_path () );
    gtk_file_chooser_set_do_overwrite_confirmation ( GTK_FILE_CHOOSER ( dialog ), TRUE);
    gtk_window_set_position ( GTK_WINDOW ( dialog ), GTK_WIN_POS_CENTER_ON_PARENT );

    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );
    if ( !nom_fichier_comptes )
    {
        gchar* tmp_str;

        tmp_str = g_strconcat ( grisbi_app_get_active_file_title (), ".gsb", NULL );
        gtk_file_chooser_set_current_name ( GTK_FILE_CHOOSER ( dialog ), tmp_str);
        g_free ( tmp_str );
    }
    else
        gtk_file_chooser_select_filename ( GTK_FILE_CHOOSER (dialog), nom_fichier_comptes );

    g_free ( nom_fichier_comptes );

    result = gtk_dialog_run ( GTK_DIALOG ( dialog ));

    switch ( result )
    {
	case GTK_RESPONSE_OK :
	    new_name = file_selection_get_filename ( GTK_FILE_CHOOSER ( dialog ));
	    gsb_file_update_last_path (file_selection_get_last_directory (GTK_FILE_CHOOSER (dialog), TRUE));
	    gtk_widget_destroy ( GTK_WIDGET ( dialog ));
	    break;

	default :
	    gtk_widget_destroy ( GTK_WIDGET ( dialog ));
	    return NULL;
    }

    if ( ! g_strrstr ( new_name, "." ) )
    {
        gchar* tmpstr = new_name;

        new_name = g_strconcat ( tmpstr, ".gsb", NULL );
        g_free ( tmpstr );
    }

    return new_name;
}



/**
 * close the file
 * if no file loaded or no change, directly return TRUE
 *
 * \param
 *
 * \return FALSE if problem, TRUE if ok
 * */
gboolean gsb_file_close ( void )
{
    gchar *nom_fichier_comptes = NULL;
    gint result;
    GrisbiWindowEtat *etat;

    devel_debug (NULL);

    if ( !assert_account_loaded () )
        return ( TRUE );

    etat = grisbi_window_get_struct_etat ();

    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );

    /* ask for saving */
    result = gsb_file_dialog_save ( nom_fichier_comptes );

    switch ( result )
    {
    case GTK_RESPONSE_OK:

        /* try to save */
        if ( !gsb_file_save_file (-1) )
        {
            g_free ( nom_fichier_comptes );

            return ( FALSE );
        }

    case GTK_RESPONSE_NO :
        /* remove the lock */
        if ( !etat->fichier_deja_ouvert
         &&
         gsb_data_account_get_accounts_amount ()
         &&
         nom_fichier_comptes )
            gsb_file_util_modify_lock ( FALSE );
        g_free ( nom_fichier_comptes );

        /* free all the variables */
/*        init_variables ();
        gsb_account_property_clear_config ( );
*/
        grisbi_window_set_active_title ( -1 );

        menus_sensitifs ( FALSE );

        table_etat = NULL;

        return ( TRUE );

    default :
        return FALSE;
    }
}


/**
 * append a new name to the list of recently opened file
 *
 * \param path_fichier
 *
 * \return
 * */
void gsb_file_append_name_to_opened_list ( gchar *path_fichier )
{
    gint i, position;
    gchar *dernier, *real_name;
    GrisbiAppConf *conf;

    devel_debug (path_fichier);

    conf = grisbi_app_get_conf ( );

    if ( !path_fichier )
        return;

    if ( conf->nb_max_derniers_fichiers_ouverts == 0 )
        return;

    if ( conf->nb_derniers_fichiers_ouverts < 0 )
        conf->nb_derniers_fichiers_ouverts = 0;

    real_name = g_strdup ( path_fichier );

    /* on commence par vérifier si ce fichier n'est pas dans les nb_derniers_fichiers_ouverts 
     * noms */
    position = 0;

    if ( conf->nb_derniers_fichiers_ouverts )
    {
        for ( i = 0; i < conf->nb_derniers_fichiers_ouverts; i++ )
        {
            if ( !strcmp ( real_name, conf->tab_noms_derniers_fichiers_ouverts[i] ) )
            {
                /* si ce fichier est déjà le dernier ouvert, on laisse tomber */
                if ( !i )
                {
                    g_free ( real_name );
                    return;
                }

                position = i;
            }
        }

        efface_derniers_fichiers_ouverts ( );

        if ( position )
        {
            /* le fichier a été trouvé, on fait juste une rotation */
            for ( i = position; i > 0 ; i-- )
                conf->tab_noms_derniers_fichiers_ouverts[i] = conf->tab_noms_derniers_fichiers_ouverts[i-1];
            if ( real_name )
                conf->tab_noms_derniers_fichiers_ouverts[0] = my_strdup ( real_name );
            else
                conf->tab_noms_derniers_fichiers_ouverts[0] = my_strdup ( "<no file>" );

            affiche_derniers_fichiers_ouverts ( );
            g_free ( real_name );

            return;
        }
        /* le fichier est nouveau, on décale tout d'un cran et on met le nouveau à 0 */

        /* si on est déjà au max, c'est juste un décalage avec perte du dernier */
        /* on garde le petit dernier dans le cas contraire */
        dernier = conf->tab_noms_derniers_fichiers_ouverts[conf->nb_derniers_fichiers_ouverts-1];
        for ( i = conf->nb_derniers_fichiers_ouverts - 1 ; i > 0 ; i-- )
            conf->tab_noms_derniers_fichiers_ouverts[i] = conf->tab_noms_derniers_fichiers_ouverts[i-1];
    }
    else
        dernier = NULL;

    if ( conf->nb_derniers_fichiers_ouverts < conf->nb_max_derniers_fichiers_ouverts )
    {
        conf->tab_noms_derniers_fichiers_ouverts = g_realloc ( conf->tab_noms_derniers_fichiers_ouverts,
                                            ( ++conf->nb_derniers_fichiers_ouverts ) * sizeof ( gpointer ) );

        conf->tab_noms_derniers_fichiers_ouverts[conf->nb_derniers_fichiers_ouverts-1] = dernier;
    }

    conf->tab_noms_derniers_fichiers_ouverts[0] = my_strdup ( real_name );

    affiche_derniers_fichiers_ouverts();

    g_free ( real_name );

    /* return */
}


/**
 * remove the filename from the list of recently opened file
 *
 * \param filename
 *
 * \return
 * */
void gsb_file_remove_name_from_opened_list ( gchar *filename )
{
    gint i, j;
    GrisbiAppConf *conf;

    devel_debug (filename);

    conf = grisbi_app_get_conf ( );

    efface_derniers_fichiers_ouverts();

    for ( i = 0; i < conf->nb_derniers_fichiers_ouverts; i++ )
    {
        if ( strcmp (filename, conf->tab_noms_derniers_fichiers_ouverts[i]) == 0 )
        {
            for ( j = i; ( j + 1 ) < conf->nb_derniers_fichiers_ouverts; j++ )
            {
                conf->tab_noms_derniers_fichiers_ouverts[j] = conf->tab_noms_derniers_fichiers_ouverts[j+1];
            }
            conf->nb_derniers_fichiers_ouverts--;
        }
    }
    affiche_derniers_fichiers_ouverts();
}


/**
 * efface la copie du fichier de comptes ancienne version
 *
 * \param filename  le chemein du fichier à effacer
 *
 * */
void gsb_file_save_remove_old_file ( gchar *filename )
{
    GtkWidget *dialog;
    GtkWidget *hbox;
    GtkWidget *image;
    GtkWidget *label;
    gint resultat;
    gchar *tmpstr;

    dialog = gtk_dialog_new_with_buttons ( 
                        _("Delete file copy from a previous version of grisbi"),
                        GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_NO, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_YES, GTK_RESPONSE_OK,
                        NULL );

    gtk_window_set_position ( GTK_WINDOW ( dialog ), GTK_WIN_POS_CENTER_ON_PARENT );
    gtk_window_set_resizable ( GTK_WINDOW ( dialog ), FALSE );

    hbox = gtk_hbox_new ( FALSE, 5);
    gtk_container_set_border_width ( GTK_CONTAINER( hbox ), 6 );
    gtk_box_pack_start ( GTK_BOX ( dialog_get_content_area ( dialog ) ), hbox, FALSE, FALSE, 5 );

    image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG );
    gtk_box_pack_start ( GTK_BOX ( hbox ), image, FALSE, FALSE, 5 );

    tmpstr = g_strdup_printf (
                        _("Caution, you are about to delete a file copy\n"
                        "from a previous version of grisbi.\n"
                        "\n<b>Do you want to delete this file:\n%s ?</b>"),
                        filename );

    label = gtk_label_new ( tmpstr );
    gtk_label_set_use_markup ( GTK_LABEL( label ), TRUE );
    gtk_box_pack_start ( GTK_BOX ( hbox ), label, FALSE, FALSE, 5 );
    g_free ( tmpstr );

    gtk_widget_show_all ( dialog );

    resultat = gtk_dialog_run ( GTK_DIALOG ( dialog ));

    if ( resultat == GTK_RESPONSE_OK )
        g_unlink ( filename );
    gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}


/**
 * set or unset the modified flag (run structure)
 * and sensitive or not the menu to save the file
 *
 * \param modif TRUE to set the modified flag, FALSE to unset
 *
 * \return
 */
void gsb_file_set_modified ( gboolean modified )
{
    GrisbiWindowRun *run;

    devel_debug_int (modified);

    /* If no file is loaded, do not change menu items. */
    if ( ! gsb_data_account_get_accounts_amount () )
        return;

    run = grisbi_window_get_struct_run ( NULL );
    if ( modified )
    {
        if ( ! run->file_modification )
        {
            run->file_modification = time ( NULL );
            gsb_gui_sensitive_menu_item ( "/menubar/FileMenu/Save", TRUE );
        }
    }
    else
    {
        run->file_modification = 0;
        gsb_gui_sensitive_menu_item ( "/menubar/FileMenu/Save", FALSE );
    }
}


/**
 * Tell if the current file has been modified or not
 *
 * \return TRUE if modified, FALSE otherwise
 */
gboolean gsb_file_get_modified ( void )
{
    GrisbiWindowRun *run;

    run = grisbi_window_get_struct_run ( NULL );
    if ( run->file_modification == 0 )
        return FALSE;
    else
        return TRUE;
}


/**
 * essaie de trouver le fichier dans le répertoire home de l'utilisateur
 * si le nom passé en option n'est pas un nom de fichier correct.
 *
 * \param liste de noms WARNING : seul le premier nom est utilisé
 *
 * \return TRUE if OK else FALSE
 **/
gboolean gsb_file_open_from_commandline ( GSList *file_list )
{
    gchar *filename;

    filename = (gchar *) file_list->data;

    if ( g_file_test ( filename, G_FILE_TEST_EXISTS ) )
    {
        gsb_file_open_file ( filename );

        return TRUE;
    }
    else
    {
        gchar *tmp_name = NULL;

        tmp_name = g_build_filename ( gsb_dirs_get_home_dir (), filename, NULL );

        if ( g_file_test ( tmp_name, G_FILE_TEST_EXISTS ) )
        {
            gsb_file_open_file ( tmp_name );
            g_free ( tmp_name );

            return TRUE;
        }
    }

    utils_files_display_dialog_error ( G_FILE_TEST_EXISTS, filename, NULL );

    return FALSE;
}


/**
 * get the account files path
 *
 * \param conf
 *
 * \return a const gchar with the backup path
 * */
const gchar *gsb_file_get_account_files_path ( GrisbiAppConf *conf )
{
    return conf->account_files_path;
}


/**
 * set the account files path
 *
 * \param le nom du répertoire ou NULL
 * \param GrisbiAppConf
 *
 * \return
 * */
void gsb_file_set_account_files_path ( const gchar *path,
                        GrisbiAppConf *conf )
{
    if ( conf->account_files_path )
        g_free ( conf->account_files_path );

    if ( path == NULL || strlen ( path ) == 0 )
        conf->account_files_path = my_strdup ( gsb_dirs_get_home_dir ( ) );
    else
        conf->account_files_path = my_strdup ( path );

    if ( !g_file_test ( conf->account_files_path, G_FILE_TEST_EXISTS ) )
    {
#ifdef _MSC_VER
        int mode = 0;
#else
        int mode = S_IRUSR | S_IWUSR | S_IXUSR;
#endif /*_MSC_VER */

        g_mkdir_with_parents ( conf->account_files_path, mode );
    }
}


/**
 * get the import files path
 *
 * \param conf
 *
 * \return a const gchar with the backup path
 * */
const gchar *gsb_file_get_import_files_path ( GrisbiAppConf *conf )
{
    return conf->import_files_path;
}


/**
 * set the import files path
 *
 * \param le nom du répertoire ou NULL
 * \param GrisbiAppConf
 *
 * \return
 * */
void gsb_file_set_import_files_path ( const gchar *path,
                        GrisbiAppConf *conf )
{
    if ( conf->import_files_path )
        g_free ( conf->import_files_path );

    if ( path == NULL || strlen ( path ) == 0 )
        conf->import_files_path = my_strdup ( gsb_dirs_get_home_dir ( ) );
    else
        conf->import_files_path = my_strdup ( path );

    if ( !g_file_test ( conf->import_files_path, G_FILE_TEST_EXISTS ) )
    {
#ifdef _MSC_VER
        int mode = 0;
#else
        int mode = S_IRUSR | S_IWUSR | S_IXUSR;
#endif /*_MSC_VER */

        g_mkdir_with_parents ( conf->import_files_path, mode );
    }
}


/* Local Variables: */
/* c-basic-offset: 4 */
/* End: */
