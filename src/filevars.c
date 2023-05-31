/*============================================================================
MIT License

Copyright (c) 2023 Trevor Monk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
============================================================================*/

/*!
 * @defgroup filevars filevars
 * @brief Map variables to template files
 * @{
 */

/*==========================================================================*/
/*!
@file filevars.c

    File Variables

    The filevars Application maps variables to template files
    using a JSON object definition to describe the mapping

*/
/*==========================================================================*/

/*============================================================================
        Includes
============================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <varserver/varserver.h>
#include <varserver/vartemplate.h>
#include <tjson/json.h>

/*============================================================================
        Private definitions
============================================================================*/


/*! fileVar component which maps a system variable to
 *  a template file */
typedef struct fileVar
{
    /*! variable handle */
    VAR_HANDLE hVar;

    /*! template file name */
    char *pFilename;

    /*! pointer to the next file variable */
    struct fileVar *pNext;

} FileVar;


/*! FileVars state */
typedef struct fileVarsState
{
    /*! variable server handle */
    VARSERVER_HANDLE hVarServer;

    /*! verbose flag */
    bool verbose;

    /*! name of the FileVars definition file */
    char *pFileName;

    /*! pointer to the file vars list */
    FileVar *pFileVars;
} FileVarsState;

/*============================================================================
        Private file scoped variables
============================================================================*/

/*! FileVars state object */
FileVarsState state;

/*============================================================================
        Private function declarations
============================================================================*/

void main(int argc, char **argv);
static int ProcessOptions( int argC, char *argV[], FileVarsState *pState );
static void usage( char *cmdname );
static int SetupFileVar( JNode *pNode, void *arg );
static int PrintFileVar( FileVarsState *pState, VAR_HANDLE hVar, int fd );
static void SetupTerminationHandler( void );
static void TerminationHandler( int signum, siginfo_t *info, void *ptr );

/*============================================================================
        Private function definitions
============================================================================*/

/*==========================================================================*/
/*  main                                                                    */
/*!
    Main entry point for the filevars application

    The main function starts the filevars application

    @param[in]
        argc
            number of arguments on the command line
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @return none

============================================================================*/
void main(int argc, char **argv)
{
    VARSERVER_HANDLE hVarServer = NULL;
    VAR_HANDLE hVar;
    int result;
    JNode *config;
    JArray *cfg;
    int sigval;
    int sig;
    int fd;

    /* clear the filevars state object */
    memset( &state, 0, sizeof( state ) );

    if( argc < 2 )
    {
        usage( argv[0] );
        exit( 1 );
    }

    /* set up the abnormal termination handler */
    SetupTerminationHandler();

    /* process the command line options */
    ProcessOptions( argc, argv, &state );

    /* process the input file */
    config = JSON_Process( state.pFileName );

    /* get the configuration array */
    cfg = (JArray *)JSON_Find( config, "config" );

    /* get a handle to the VAR server */
    state.hVarServer = VARSERVER_Open();
    if( state.hVarServer != NULL )
    {
        /* set up the file vars by iterating through the configuration array */
        JSON_Iterate( cfg, SetupFileVar, (void *)&state );

        while( 1 )
        {
            /* wait for a signal from the variable server */
            sig = VARSERVER_WaitSignal( &sigval );
            if( sig == SIG_VAR_PRINT )
            {
                /* open a print session */
                VAR_OpenPrintSession( state.hVarServer,
                                      sigval,
                                      &hVar,
                                      &fd );

                /* print the file variable */
                PrintFileVar( &state, hVar, fd );

                /* Close the print session */
                VAR_ClosePrintSession( state.hVarServer,
                                       sigval,
                                       fd );
            }
        }

        /* close the variable server */
        VARSERVER_Close( state.hVarServer );
    }
}

/*============================================================================*/
/*  SetupFileVar                                                              */
/*!
    Set up a filevar object

    The SetupFileVar function is a callback function for the JSON_Iterate
    function which sets up a file variable from the JSON configuration.
    The file variable definition object is expected to look as follows:

    { "name": "varname", "file": "filename" }

    @param[in]
       pNode
            pointer to the FileVar node

    @param[in]
        arg
            opaque pointer argument used for the filevar state object

    @retval EOK - the file variable was set up successfully
    @retval EINVAL - the file variable could not be set up

==============================================================================*/
static int SetupFileVar( JNode *pNode, void *arg )
{
    FileVarsState *pState = (FileVarsState *)arg;
    JVar *pName;
    JVar *pFileName;
    char *varname = NULL;
    char *filename = NULL;
    VARSERVER_HANDLE hVarServer;
    FileVar *pFilevar;
    int result = EINVAL;

    if( pState != NULL )
    {
        /* get a handle to the VarServer */
        hVarServer = pState->hVarServer;
        pName = (JVar *)JSON_Find( pNode, "var" );
        if( pName != NULL )
        {
            varname = pName->var.val.str;
        }

        pFileName = (JVar *)JSON_Find( pNode, "file" );
        if( pFileName != NULL )
        {
            filename = pFileName->var.val.str;
        }

        if( ( varname != NULL ) &&
            ( filename != NULL ) )
        {
            /* allocate memory for the file variable */
            pFilevar = malloc( sizeof( FileVar ) );
            if( pFilevar != NULL )
            {
                pFilevar->hVar = VAR_FindByName( hVarServer, varname );
                pFilevar->pFilename = strdup( filename );

                VAR_Notify( hVarServer, pFilevar->hVar, NOTIFY_PRINT );

                pFilevar->pNext = pState->pFileVars;
                pState->pFileVars = pFilevar;

                result = EOK;
            }
        }
    }

    return result;
}

/*============================================================================*/
/*  PrintFileVar                                                              */
/*!
    Print a filevar

    The PrintFileVar function iterates through all the registered filevars
    looking for the specified variable handle.  If found, it renders the
    template file associated with the file handle to the specified output
    stream.

    @param[in]
       pState
            pointer to the FileVars state object

    @param[in]
        hVar
            handle of the variable to print

    @param[in]
        fd
            output file descriptor to render to

    @retval EOK - file variable rendered successfully
    @retval ENOENT - file variable was not found
    @retval EINVAL - invalid arguments

============================================================================*/
static int PrintFileVar( FileVarsState *pState, VAR_HANDLE hVar, int fd )
{
    int result = EINVAL;
    FileVar *pFileVar;
    int fd_in;

    if( ( pState != NULL ) &&
        ( hVar != VAR_INVALID ) )
    {
        result = ENOENT;

        pFileVar = pState->pFileVars;
        while( pFileVar != NULL )
        {
            if( pFileVar->hVar == hVar )
            {
                fd_in = open( pFileVar->pFilename, O_RDONLY );
                if( fd_in > 0 )
                {
                    TEMPLATE_FileToFile( pState->hVarServer, fd_in, fd );
                    close( fd_in );
                }

                result = EOK;
                break;
            }

            pFileVar = pFileVar->pNext;
        }
    }

    return result;
}

/*==========================================================================*/
/*  usage                                                                   */
/*!
    Display the application usage

    The usage function dumps the application usage message
    to stderr.

    @param[in]
       cmdname
            pointer to the invoked command name

    @return none

============================================================================*/
static void usage( char *cmdname )
{
    if( cmdname != NULL )
    {
        fprintf(stderr,
                "usage: %s [-v] [-h] "
                " [-h] : display this help"
                " [-v] : verbose output"
                " -f <filename> : configuration file",
                cmdname );
    }
}

/*==========================================================================*/
/*  ProcessOptions                                                          */
/*!
    Process the command line options

    The ProcessOptions function processes the command line options and
    populates the FileVarState object

    @param[in]
        argC
            number of arguments
            (including the command itself)

    @param[in]
        argv
            array of pointers to the command line arguments

    @param[in]
        pState
            pointer to the FileVars state object

    @return always 0

============================================================================*/
static int ProcessOptions( int argC, char *argV[], FileVarsState *pState )
{
    int c;
    int result = EINVAL;
    const char *options = "hvf:";

    if( ( pState != NULL ) &&
        ( argV != NULL ) )
    {
        while( ( c = getopt( argC, argV, options ) ) != -1 )
        {
            switch( c )
            {
                case 'v':
                    pState->verbose = true;
                    break;

                case 'h':
                    usage( argV[0] );
                    break;

                case 'f':
                    pState->pFileName = strdup(optarg);
                    break;

                default:
                    break;

            }
        }
    }

    return 0;
}

/*============================================================================*/
/*  SetupTerminationHandler                                                   */
/*!
    Set up an abnormal termination handler

    The SetupTerminationHandler function registers a termination handler
    function with the kernel in case of an abnormal termination of this
    process.

==============================================================================*/
static void SetupTerminationHandler( void )
{
    static struct sigaction sigact;

    memset( &sigact, 0, sizeof(sigact) );

    sigact.sa_sigaction = TerminationHandler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction( SIGTERM, &sigact, NULL );
    sigaction( SIGINT, &sigact, NULL );

}

/*============================================================================*/
/*  TerminationHandler                                                        */
/*!
    Abnormal termination handler

    The TerminationHandler function will be invoked in case of an abnormal
    termination of this process.  The termination handler closes
    the connection with the variable server and cleans up its VARFP shared
    memory.

@param[in]
    signum
        The signal which caused the abnormal termination (unused)

@param[in]
    info
        pointer to a siginfo_t object (unused)

@param[in]
    ptr
        signal context information (ucontext_t) (unused)

==============================================================================*/
static void TerminationHandler( int signum, siginfo_t *info, void *ptr )
{
    VARSERVER_Close( state.hVarServer );

    syslog( LOG_ERR, "Abnormal termination of filevars" );

    exit( 1 );
}

/*! @}
 * end of filevars group */
