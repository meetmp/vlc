/*****************************************************************************
 * configuration_chain.c : configuration module chain parsing stuff
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* chain format:
    module{option=*:option=*}[:module{option=*:...}]
 */
#define SKIPSPACE( p ) { while( *p && ( *p == ' ' || *p == '\t' ) ) p++; }
#define SKIPTRAILINGSPACE( p, e ) \
    { while( e > p && ( *(e-1) == ' ' || *(e-1) == '\t' ) ) e--; }

/* go accross " " and { } */
static char *_get_chain_end( char *str )
{
    char c, *p = str;

    SKIPSPACE( p );

    for( ;; )
    {
        if( !*p || *p == ',' || *p == '}' ) return p;

        if( *p != '{' && *p != '"' && *p != '\'' )
        {
            p++;
            continue;
        }

        if( *p == '{' ) c = '}';
        else c = *p;
        p++;

        for( ;; )
        {
            if( !*p ) return p;

            if( *p == c ) return ++p;
            else if( *p == '{' && c == '}' ) p = _get_chain_end( p );
            else p++;
        }
    }
}

char *config_ChainCreate( char **ppsz_name, config_chain_t **pp_cfg, char *psz_chain )
{
    config_chain_t *p_cfg = NULL;
    char       *p = psz_chain;

    *ppsz_name = NULL;
    *pp_cfg    = NULL;

    if( !p ) return NULL;

    SKIPSPACE( p );

    while( *p && *p != '{' && *p != ':' && *p != ' ' && *p != '\t' ) p++;

    if( p == psz_chain ) return NULL;

    *ppsz_name = strndup( psz_chain, p - psz_chain );

    SKIPSPACE( p );

    if( *p == '{' )
    {
        char *psz_name;

        p++;

        for( ;; )
        {
            config_chain_t cfg;

            SKIPSPACE( p );

            psz_name = p;

            while( *p && *p != '=' && *p != ',' && *p != '{' && *p != '}' &&
                   *p != ' ' && *p != '\t' ) p++;

            /* fprintf( stderr, "name=%s - rest=%s\n", psz_name, p ); */
            if( p == psz_name )
            {
                fprintf( stderr, "invalid options (empty)" );
                break;
            }

            cfg.psz_name = strndup( psz_name, p - psz_name );

            SKIPSPACE( p );

            if( *p == '=' || *p == '{' )
            {
                char *end;
                vlc_bool_t b_keep_brackets = (*p == '{');

                if( *p == '=' ) p++;

                end = _get_chain_end( p );
                if( end <= p )
                {
                    cfg.psz_value = NULL;
                }
                else
                {
                    /* Skip heading and trailing spaces.
                     * This ain't necessary but will avoid simple
                     * user mistakes. */
                    SKIPSPACE( p );
                }

                if( end <= p )
                {
                    cfg.psz_value = NULL;
                }
                else
                {
                    if( *p == '\'' || *p == '"' ||
                        ( !b_keep_brackets && *p == '{' ) )
                    {
                        p++;

                        if( *(end-1) != '\'' && *(end-1) == '"' )
                            SKIPTRAILINGSPACE( p, end );

                        if( end - 1 <= p ) cfg.psz_value = NULL;
                        else cfg.psz_value = strndup( p, end -1 - p );
                    }
                    else
                    {
                        SKIPTRAILINGSPACE( p, end );
                        if( end <= p ) cfg.psz_value = NULL;
                        else cfg.psz_value = strndup( p, end - p );
                    }
                }

                p = end;
                SKIPSPACE( p );
            }
            else
            {
                cfg.psz_value = NULL;
            }

            cfg.p_next = NULL;
            if( p_cfg )
            {
                p_cfg->p_next = malloc( sizeof( config_chain_t ) );
                memcpy( p_cfg->p_next, &cfg, sizeof( config_chain_t ) );

                p_cfg = p_cfg->p_next;
            }
            else
            {
                p_cfg = malloc( sizeof( config_chain_t ) );
                memcpy( p_cfg, &cfg, sizeof( config_chain_t ) );

                *pp_cfg = p_cfg;
            }

            if( *p == ',' ) p++;

            if( *p == '}' )
            {
                p++;
                break;
            }
        }
    }

    if( *p == ':' ) return( strdup( p + 1 ) );

    return NULL;
}

void config_ChainDestroy( config_chain_t *p_cfg )
{
    while( p_cfg != NULL )
    {
        config_chain_t *p_next;

        p_next = p_cfg->p_next;

        FREENULL( p_cfg->psz_name );
        FREENULL( p_cfg->psz_value );
        free( p_cfg );

        p_cfg = p_next;
    }
}

void __config_ChainParse( vlc_object_t *p_this, char *psz_prefix,
                      const char **ppsz_options, config_chain_t *cfg )
{
    char *psz_name;
    int  i_type;
    int  i;

    /* First, var_Create all variables */
    for( i = 0; ppsz_options[i] != NULL; i++ )
    {
        asprintf( &psz_name, "%s%s", psz_prefix,
                  *ppsz_options[i] == '*' ? &ppsz_options[i][1] : ppsz_options[i] );

        i_type = config_GetType( p_this, psz_name );

        var_Create( p_this, psz_name, i_type | VLC_VAR_DOINHERIT );
        free( psz_name );
    }

    /* Now parse options and set value */
    if( psz_prefix == NULL ) psz_prefix = "";

    while( cfg )
    {
        vlc_value_t val;
        vlc_bool_t b_yes = VLC_TRUE;
        vlc_bool_t b_once = VLC_FALSE;
        module_config_t *p_conf;

        if( cfg->psz_name == NULL || *cfg->psz_name == '\0' )
        {
            cfg = cfg->p_next;
            continue;
        }
        for( i = 0; ppsz_options[i] != NULL; i++ )
        {
            if( !strcmp( ppsz_options[i], cfg->psz_name ) )
            {
                break;
            }
            if( ( !strncmp( cfg->psz_name, "no-", 3 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 3 ) ) ||
                ( !strncmp( cfg->psz_name, "no", 2 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 2 ) ) )
            {
                b_yes = VLC_FALSE;
                break;
            }

            if( *ppsz_options[i] == '*' &&
                !strcmp( &ppsz_options[i][1], cfg->psz_name ) )
            {
                b_once = VLC_TRUE;
                break;
            }

        }
        if( ppsz_options[i] == NULL )
        {
            msg_Warn( p_this, "option %s is unknown", cfg->psz_name );
            cfg = cfg->p_next;
            continue;
        }

        /* create name */
        asprintf( &psz_name, "%s%s", psz_prefix, b_once ? &ppsz_options[i][1] : ppsz_options[i] );

        /* Check if the option is deprecated */
        p_conf = config_FindConfig( p_this, psz_name );

        /* This is basically cut and paste from src/misc/configuration.c
         * with slight changes */
        if( p_conf && p_conf->psz_current )
        {
            if( !strcmp( p_conf->psz_current, "SUPPRESSED" ) )
            {
                msg_Err( p_this, "Option %s is no longer used.",
                         p_conf->psz_name );
                goto next;
            }
            else if( p_conf->b_strict )
            {
                msg_Err( p_this, "Option %s is deprecated. Use %s instead.",
                         p_conf->psz_name, p_conf->psz_current );
                /* TODO: this should return an error and end option parsing
                 * ... but doing this would change the VLC API and all the
                 * modules so i'll do it later */
                goto next;
            }
            else
            {
                msg_Warn( p_this, "Option %s is deprecated. You should use "
                        "%s instead.", p_conf->psz_name, p_conf->psz_current );
                free( psz_name );
                psz_name = strdup( p_conf->psz_current );
            }
        }
        /* </Check if the option is deprecated> */

        /* get the type of the variable */
        i_type = config_GetType( p_this, psz_name );
        if( !i_type )
        {
            msg_Warn( p_this, "unknown option %s (value=%s)",
                      cfg->psz_name, cfg->psz_value );
            goto next;
        }
        if( i_type != VLC_VAR_BOOL && cfg->psz_value == NULL )
        {
            msg_Warn( p_this, "missing value for option %s", cfg->psz_name );
            goto next;
        }
        if( i_type != VLC_VAR_STRING && b_once )
        {
            msg_Warn( p_this, "*option_name need to be a string option" );
            goto next;
        }

        switch( i_type )
        {
            case VLC_VAR_BOOL:
                val.b_bool = b_yes;
                break;
            case VLC_VAR_INTEGER:
                val.i_int = strtol( cfg->psz_value ? cfg->psz_value : "0",
                                    NULL, 0 );
                break;
            case VLC_VAR_FLOAT:
                val.f_float = atof( cfg->psz_value ? cfg->psz_value : "0" );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
                val.psz_string = cfg->psz_value;
                break;
            default:
                msg_Warn( p_this, "unhandled config var type" );
                memset( &val, 0, sizeof( vlc_value_t ) );
                break;
        }
        if( b_once )
        {
            vlc_value_t val2;

            var_Get( p_this, psz_name, &val2 );
            if( *val2.psz_string )
            {
                free( val2.psz_string );
                msg_Dbg( p_this, "ignoring option %s (not first occurrence)", psz_name );
                goto next;
            }
            free( val2.psz_string );
        }
        var_Set( p_this, psz_name, val );
        msg_Dbg( p_this, "set config option: %s to %s", psz_name, cfg->psz_value );

    next:
        free( psz_name );
        cfg = cfg->p_next;
    }
}
