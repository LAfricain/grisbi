/* ************************************************************************** */
/*                                                                            */
/*                                  erreur.c                                  */
/*                                                                            */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2003-2008 Benjamin Drieu (bdrieu@april.org)                       */
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

#include "include.h"
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "erreur.h"
#include "dialog.h"
#include "grisbi_app.h"
#include "gsb_dirs.h"
#include "gsb_file.h"
#include "gsb_file_save.h"
#include "gsb_file_util.h"
#include "gsb_status.h"
#include "import.h"
#include "gsb_locale.h"
#include "utils_str.h"
/*END_INCLUDE*/

#ifdef HAVE_BACKTRACE
#include "execinfo.h"
#endif


/*START_STATIC*/
static gchar *get_debug_time ( void );
static GtkWidget *print_backtrace ( void );
/*END_STATIC*/

/*START_EXTERN*/
/*END_EXTERN*/

/* niveau de debug */
static gint debugging_grisbi;

/* mode debug pour le fichier de compte */
static gboolean debug_mode = FALSE;

/* path and name of the file containing the log when debug mode is active
 * this values should not be freed when begin a new file to continue the log */
static FILE *debug_file = NULL;

/**
 * Traitement des erreurs de segmentation
 *
 * \param gint numéro de signal
 *
 * \return
 */
void traitement_sigsegv ( gint signal_nb )
{
    gchar *errmsg = g_strdup ( "" );
    gchar *old_errmsg;
    gchar *nom_fichier_comptes = NULL;
    gchar *long_filename = NULL;
    GtkWidget *dialog;
    gboolean compress_file = FALSE;
    gboolean is_loading = FALSE;
    gboolean is_saving = FALSE;
    GrisbiApp *app = NULL;
#ifdef HAVE_BACKTRACE
    GtkWidget *expander;
#endif
devel_debug (NULL);

    app = grisbi_app_get_default ( TRUE );
    if ( app )
    {
        GrisbiWindow *window;
        const gchar *gsb_file_default_dir;

        window = grisbi_app_get_active_window ( app );
        if ( window )
            nom_fichier_comptes = g_strdup ( grisbi_window_get_filename ( window ) );
        gsb_file_default_dir = gsb_dirs_get_home_dir ( );

        if ( nom_fichier_comptes )
        {
            gchar *tmp_str2;
            gchar *tmp_str;
            GrisbiAppConf *conf = NULL;
            GrisbiWindowRun *run = NULL;

            conf = grisbi_app_get_conf ( );
            grisbi_window_get_struct_run ( NULL );

            compress_file = conf->compress_file;
            is_loading = run->is_loading;
            is_saving = run->is_saving;

            /* set # around the filename */
            tmp_str = g_path_get_basename ( nom_fichier_comptes );
            tmp_str2 = g_strconcat ( "#", tmp_str, "#", NULL );
            long_filename = g_build_filename ( gsb_file_default_dir, tmp_str2, NULL );
            g_free ( tmp_str );
            g_free ( tmp_str2 );
        }
        else
            long_filename = g_build_filename ( gsb_file_default_dir, "#grisbi_save_no_name.gsb#", NULL );
    }

    /* il y a 4 possibilités :
     *  - Demande de fermeture de la part du système
     *  - Chargement d'un fichier -> celui-ci est corrompu
     *  - Sauvegarde d'un fichier -> on peut rien faire
     *  - Erreur de mémoire -> tentative de sauver le fichier sous le nom entouré de #
     */
    if ( ( signal_nb == SIGINT || signal_nb == SIGTERM ) && gsb_file_get_modified () )
    {
        gint res;

        old_errmsg = g_strdup ( _("Request for forced shutdown of  Grisbi\n") );
        errmsg = g_markup_printf_escaped ( _("The file '%s' has been modified. Do you want to save it?\n"),
                                    long_filename );

        dialog = gtk_message_dialog_new ( GTK_WINDOW ( grisbi_app_get_active_window ( NULL ) ),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_YES_NO,
                                    old_errmsg );
        gtk_dialog_set_default_response     ( GTK_DIALOG ( dialog ), GTK_RESPONSE_YES );
        gtk_message_dialog_format_secondary_markup ( GTK_MESSAGE_DIALOG ( dialog ), "%s", errmsg );

        g_free ( old_errmsg );
        g_free ( errmsg );

        res = gtk_dialog_run ( GTK_DIALOG ( dialog ) );

        if ( res == GTK_RESPONSE_YES )
        {
            gsb_status_message ( _("Save file") );
            gsb_file_save_save_file ( long_filename, compress_file, FALSE );
            gsb_status_clear ();
        }

        gtk_widget_destroy ( dialog );
        gsb_file_util_modify_lock ( FALSE );
        g_free ( nom_fichier_comptes );
        g_free ( long_filename );

        exit ( 0 );
    }
    else if ( nom_fichier_comptes && ( is_loading || is_saving || !gsb_file_get_modified ( ) ) )
    {
        if ( is_loading )
        {
            old_errmsg = errmsg;
            errmsg = g_strconcat ( errmsg, _("File is corrupted."), NULL );
            g_free ( old_errmsg );
        }

        if ( is_saving )
        {
            old_errmsg = errmsg;
            errmsg = g_strconcat ( errmsg, _("Error occured saving file."), NULL );
            g_free ( old_errmsg );
        }
    }
    else
    {
        /* c'est un bug pendant le fonctionnement de Grisbi s'il n'y a
           pas de nom de fichier, on le crée, sinon on rajoute #
           autour */
        if ( long_filename )
        {
            gsb_status_message ( _("Save file") );
            gsb_file_save_save_file ( long_filename, compress_file, FALSE );
            gsb_status_clear ( );

            old_errmsg = errmsg;
            errmsg = g_strconcat ( errmsg,
                        g_strdup_printf ( _("Grisbi made a backup file at '%s'."),
                        long_filename ),
                        NULL );
            g_free ( old_errmsg );
            g_free ( nom_fichier_comptes );
            g_free ( long_filename );
        }
    }

    old_errmsg = errmsg;
    errmsg = g_strconcat ( errmsg,
                        "\n\n",
                        _("Please report this problem to <tt>http://www.grisbi.org/bugtracking/</tt>.  "),
                        NULL );
     g_free ( old_errmsg );

#ifdef HAVE_BACKTRACE
    old_errmsg = errmsg;
    errmsg = g_strconcat ( errmsg,
                        _("Copy and paste the following backtrace with your bug "
                        "report."),
                        NULL );
     g_free ( old_errmsg );
#endif

    dialog = dialogue_special_no_run ( GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                        make_hint ( _("Grisbi terminated due to a segmentation fault."),
                        errmsg ) );
    g_free ( errmsg );

#ifdef HAVE_BACKTRACE
    {
        gchar *tmp_str;

        tmp_str = g_strconcat ( "<b>", _("Backtrace"), "</b>", NULL );
        expander = gtk_expander_new ( tmp_str );
        g_free ( tmp_str );

        gtk_expander_set_use_markup ( GTK_EXPANDER ( expander ), TRUE );
        gtk_container_add ( GTK_CONTAINER ( expander ), print_backtrace() );
        gtk_box_pack_start ( GTK_BOX ( dialog_get_content_area ( dialog ) ), expander, FALSE, FALSE, 6 );

        gtk_widget_show_all ( dialog );
    }
#endif
    gtk_dialog_run ( GTK_DIALOG ( dialog ) );

    /*     on évite le message du fichier ouvert à la prochaine ouverture */

    gsb_file_util_modify_lock ( FALSE );

    exit(1);
}

/**
 * initialise le niveau de debug de grisbi
 *
 * \param niveau de débug entré en ligne de commande
 *
 * \return
 */
void initialize_debugging ( gint cde_line_debug_level )
{
    /* un int pour stocker le level de debug et une chaine qui contient sa version texte */
    gint number = 0;
    gchar *debug_level = "";
    gchar *tmpstr1;
    gchar *tmpstr2;

    if ( IS_DEVELOPMENT_VERSION )
        number = 1;

    /* on garde le niveau mini de débogage si $DEBUG_GRISBI n'existe pas */
    if ( number )
    {
        if ( getenv ( "DEBUG_GRISBI" ) )
            number = utils_str_atoi ( getenv ( "DEBUG_GRISBI" ) );
    }
    if ( cde_line_debug_level == -1 )
        debugging_grisbi = number;
    else
        debugging_grisbi = cde_line_debug_level;

    if ( debugging_grisbi > MAX_DEBUG_LEVEL )
        debugging_grisbi = MAX_DEBUG_LEVEL;

    if ( debugging_grisbi )
    {
        /* on renseigne le texte du level de debug */
        switch ( debugging_grisbi )
        {
            case DEBUG_LEVEL_ALERT: { debug_level = "Alert"; break; }
            case DEBUG_LEVEL_IMPORTANT: { debug_level = "Important"; break; }
            case DEBUG_LEVEL_NOTICE: { debug_level = "Notice"; break; }
            case DEBUG_LEVEL_INFO: { debug_level = "Info"; break; }
            case DEBUG_LEVEL_DEBUG: { debug_level = "Debug"; break; }
        }

        /* on affiche un message de debug pour indiquer que le debug est actif */
        tmpstr1 = g_strdup_printf ( _("GRISBI %s Debug"),VERSION );
        tmpstr2 = g_strdup_printf ( _("Debug enabled, level is '%s'"), debug_level );
        debug_message_string ( tmpstr1,
                        __FILE__, __LINE__, __PRETTY_FUNCTION__,
                        tmpstr2,
                        DEBUG_LEVEL_INFO,
                        TRUE );
        g_free ( tmpstr1 );
        g_free ( tmpstr2 );
    }
}


/**
 * renvoie le niveau de débug
 *
 * \param
 *
 * \return debug_level
 */
gint gsb_debug_get_debug_level ( void )
{
    return debugging_grisbi;
}


/**
 * renvoie TRUE si le mode de déboguage est actif
 *
 * \param
 *
 * \return debug_mode
 */
gboolean gsb_debug_get_debug_mode ( void )
{
    return debug_mode;
}


/**
 * return a string with the current time
 * use to debug lines
 *
 * \param
 *
 * \return a string with the date and time, don't free it
 * */
gchar *get_debug_time ( void )
{
    /* le temps courant et une chaine dans laquelle on stocke le temps courant */
    time_t debug_time;
    gchar *str_debug_time;

    /* on choppe le temps courant et on va le mettre dans une chaine */
    time(&debug_time);
    str_debug_time=ctime(&debug_time);

    /* on fait sauter le retour a la ligne */
    str_debug_time[strlen(str_debug_time) - 1] = '\0';

    /* on renvoit le temps */
    return str_debug_time;
}



/**
 * show a debug message in the terminal
 * only if debug mode is on
 * not called directly so need to force the extern 
 * the param to chow is a string
 *
 * \param
 * \param
 * \param
 * \param
 * \param message a string to display with the message, NULL if no message
 * \param
 * \param
 *
 * \return
 * */
G_MODULE_EXPORT void debug_message_string ( gchar *prefixe,
                        gchar *file,
                        gint line,
                        const char *function,
                        const gchar *message,
                        gint level,
                        gboolean force_debug_display )
{
    /* il faut bien entendu que le mode debug soit actif ou que l'on force l'affichage */
    if ( ( debugging_grisbi && level <= debugging_grisbi) || force_debug_display || debug_mode )
    {
        gchar* tmp_str;

        /* on affiche dans la console le message */
        if (message)
            tmp_str = g_strdup_printf(_("%s, %2f : %s - %s:%d:%s - %s\n"),
                        get_debug_time (), (double )clock()/ CLOCKS_PER_SEC, prefixe,
                        file, line, function, message);
        else
            tmp_str = g_strdup_printf(_("%s, %2f : %s - %s:%d:%s\n"),
                        get_debug_time (), (double )clock()/ CLOCKS_PER_SEC, prefixe,
                        file, line, function);

        if ( debug_mode )
        {
            fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
            fflush ( debug_file );
        }

        g_print( "%s", tmp_str );
        g_free ( tmp_str );
    }
}

/**
 * show a debug message in the terminal
 * only if debug mode is on
 * not called directly so need to force the extern 
 * the param to chow is a number
 *
 * \param
 * \param
 * \param
 * \param
 * \param message a number to display with the message
 * \param
 * \param
 *
 * \return
 * */
void debug_message_int ( gchar *prefixe,
                        gchar *file,
                        gint line,
                        const char *function,
                        gint message,
                        gint level,
                        gboolean force_debug_display )
{
    /* il faut bien entendu que le mode debug soit actif ou que l'on force l'affichage */
    if ( ( debugging_grisbi && level <= debugging_grisbi) || force_debug_display || debug_mode )
    {
        gchar* tmp_str;

        /* on affiche dans la console le message */
        tmp_str = g_strdup_printf(_("%s, %2f : %s - %s:%d:%s - %d\n"),
                        get_debug_time (), (double )clock()/ CLOCKS_PER_SEC, prefixe,
                        file, line, function, message);

        if (debug_mode)
        {
            fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
            fflush ( debug_file );
        }

        g_print( "%s", tmp_str );
        g_free ( tmp_str );
    }
}


/**
 * show a debug message in the terminal
 * only if debug mode is on
 * not called directly so need to force the extern 
 * the param to chow is a number
 *
 * \param
 * \param
 * \param
 * \param
 * \param message a number to display with the message
 * \param
 * \param
 *
 * \return
 * */
void debug_message_real ( gchar *prefixe,
                        gchar *file,
                        gint line,
                        const char *function,
                        gsb_real message,
                        gint level,
                        gboolean force_debug_display )
{
   /* il faut bien entendu que le mode debug soit actif ou que l'on force l'affichage */
    if ( ( debugging_grisbi && level <= debugging_grisbi) || force_debug_display || debug_mode )
    {
        gchar* tmp_str;

        /* on affiche dans la console le message */
        tmp_str = g_strdup_printf ("%s, %2f : %s - %s:%d:%s - %"G_GINT64_MODIFIER"d E %d\n",
                        get_debug_time (), (double )clock()/ CLOCKS_PER_SEC, prefixe,
                        file, line, function, message.mantissa, message.exponent );

        if ( debug_mode )
        {
            fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
            fflush ( debug_file );
        }

        g_print( "%s", tmp_str );
        g_free ( tmp_str );
    }
}




/**
 * Print the backtrace upon segfault.
 */
GtkWidget *print_backtrace ( void )
{
#ifdef HAVE_BACKTRACE
    void *backtrace_content[15];
    int backtrace_size;
    size_t i;
    gchar **backtrace_strings, *text = g_strdup("");
    GtkWidget * label;

    backtrace_size = backtrace (backtrace_content, 15);
    backtrace_strings = backtrace_symbols (backtrace_content, backtrace_size);

    g_print ("%s : %d elements in stack.\n", get_debug_time(), backtrace_size);

    for ( i = 0; i < backtrace_size; i++ )
    {
        gchar* old_text;

        g_print ("\t%s\n", backtrace_strings[i]);
        old_text = text;
        text = g_strconcat ( text, "\t", backtrace_strings[i], "\n",  NULL );

        g_free ( old_text );
    }

    label = gtk_label_new ( text );
    g_free ( text );
    gtk_label_set_selectable ( GTK_LABEL ( label ), TRUE );
    return label;
#else
    return NULL;
#endif
}

/**
 * called by menu : begin the debug mode
 * show a message to say where the log will be saved
 *
 * \param
 *
 * \return FALSE
 * */
gboolean gsb_debug_start_log ( void )
{
    gchar *tmp_str;
    gchar *debug_filename;
    gchar *nom_fichier_comptes;

    devel_debug ( NULL );

    nom_fichier_comptes = g_strdup ( grisbi_app_get_active_filename () );

    if ( nom_fichier_comptes )
    {
        gchar *base_filename = g_strdup ( nom_fichier_comptes );
        gchar *complete_filename;
        gchar *basename;

        base_filename [strlen ( base_filename ) - 4] = 0;
        complete_filename = g_strconcat ( base_filename, "-log.txt", NULL);
        basename = g_path_get_basename ( complete_filename );

        debug_filename = g_build_filename ( gsb_dirs_get_home_dir (), basename, NULL);

        g_free ( basename);
        g_free ( complete_filename );
        g_free ( base_filename );
        g_free ( nom_fichier_comptes );
    }
    else
    {
        debug_filename = g_build_filename ( gsb_dirs_get_home_dir (), "No_name-log.txt", NULL);
    }


    tmp_str = g_strdup_printf (_("The debug-mode is starting. Grisbi will write a log into %s. "
                        "Please send that file with the obfuscated file into the bug report."),
                        debug_filename );

    dialogue ( tmp_str );
    g_free (tmp_str);

    debug_file = g_fopen ( debug_filename, "w" );

    g_free ( debug_filename );

    if ( debug_file )
    {
        GtkWidget *widget;
        gchar *tmp_str_2;
        GtkUIManager *ui_manager;

        ui_manager = grisbi_window_get_ui_manager ( grisbi_app_get_active_window ( NULL ) );

        widget = gtk_ui_manager_get_widget ( ui_manager, "/menubar/FileMenu/DebugMode" );
        debug_mode = TRUE;

        /* unsensitive the menu, we cannot reverse the debug mode */
        if ( widget && GTK_IS_WIDGET ( widget ) )
            gtk_widget_set_sensitive ( widget, FALSE );

        /* début du mode débogage */
        tmp_str = g_strdup_printf(_("%s, %2f : Debug - %s:%d:%s\n\n"),
                        get_debug_time ( ),
                        (double ) clock ( )/ CLOCKS_PER_SEC,
                        __FILE__,
                        __LINE__,
                        __PRETTY_FUNCTION__ );
        fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );

        /* write locales */
        tmp_str = gsb_locale_get_print_locale_var ( );

        fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );

        tmp_str = g_strdup_printf ( "gint64\n"
                        "\tG_GINT64_MODIFIER = \"%s\"\n"
                        "\t%"G_GINT64_MODIFIER"d\n\n",
                        G_GINT64_MODIFIER, G_MAXINT64 );

        fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );

        tmp_str = gsb_dirs_get_print_dir_var ( );

        fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );

        tmp_str = g_strdup ( "Formats importés\n" );
        fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );

        tmp_str = gsb_import_formats_get_list_formats_to_string ( );
        tmp_str_2 = g_strconcat ( tmp_str, "\n", NULL );

        fwrite ( tmp_str_2, sizeof (gchar), strlen ( tmp_str_2 ), debug_file );
        fflush ( debug_file );

        g_free ( tmp_str );
        g_free ( tmp_str_2 );
    }
    else
        dialogue_error (_("Grisbi failed to create the log file...") );

    return FALSE;
}


/**
 *
 * */
void gsb_debug_finish_log ( void )
{
    if ( debug_file )
        fclose ( debug_file );
}


/**
 *
 *
 *
 */
void debug_print_log_string ( gchar *prefixe,
                        gchar *file,
                        gint line,
                        const char *function,
                        const gchar *msg )
{
    gchar *tmp_str;
    gchar *message;

    if ( debug_file == NULL )
        return;

    if ( msg && strlen ( msg ) )
        message = g_strdup ( msg );
    else
        message = g_strdup ( "(null)" );

    tmp_str = g_strdup_printf(_("%s, %2f : %s - %s:%d:%s - %s\n"),
                        get_debug_time ( ),
                        (double ) clock ( )/ CLOCKS_PER_SEC,
                        prefixe,
                        file,
                        line,
                        function,
                        message );

    fwrite ( tmp_str, sizeof (gchar), strlen ( tmp_str ), debug_file );
    fflush ( debug_file );

    g_free ( tmp_str );
    g_free ( message );
}


/* Local Variables: */
/* c-basic-offset: 4 */
/* End: */
