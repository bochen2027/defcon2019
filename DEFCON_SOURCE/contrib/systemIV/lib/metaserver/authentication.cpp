#include "lib/universal_include.h"
#include "lib/math/random_number.h"
#include "lib/math/hash.h"
#include "lib/debug_utils.h"
#include "lib/tosser/directory.h"
#include "lib/tosser/llist.h"
#include "lib/netlib/net_mutex.h"

#include "metaserver.h"
#include "metaserver_defines.h"
#include "authentication.h"


static char s_authKey[256];


class AuthenticationResult
{
public:
    char    m_ip[256];
    char    m_authKey[256];
    int     m_keyId;
    int     m_authResult;
    int     m_numTries;

    AuthenticationResult()
        :   m_keyId(-1),
			m_authResult(AuthenticationUnknown),
			m_numTries(0)
    {
    }
};


static LList<AuthenticationResult *>    s_authResults;
static NetMutex                         s_authResultsMutex;

static bool                             s_authThreadRunning = false;
static int                              s_hashToken = 0;
static bool                             s_enforceDemo = false;


void Authentication_GenerateKey( char *_key, bool _demoKey )
{
    int counter = 0;
    int total = 0;

    for( int i = 0; i < AUTHENTICATION_KEYLEN; ++i )
    {
        if( i == AUTHENTICATION_KEYLEN-1 )
        {
            // Terminator
            _key[i] = '\x0';
        }
        else if( i == AUTHENTICATION_KEYLEN-2 )
        {
            // The last byte is a checking byte
            _key[i] = 'A' + (total % 26);
        }
        else if( counter == 6 )
        {
            // Every 6th letter is a dash for easy reading
            _key[i] = '-';
            counter = 0;
        }
        else if( _demoKey && i < 4 )
        {
            // This is a DEMO key, so put DEMO at the start
            static char demoMarker[5] = "DEMO";            
            _key[i] = demoMarker[i];
            ++counter;
        }
        else
        {
            // Every other letter is random
            _key[i] = 'A' + (AppRandom() % 26);
            ++counter;
        }

        total += _key[i];
    }
}



int Authentication_SimpleKeyCheck( char *_key )
{
    if( Authentication_IsHashKey(_key) )
    {
        // This is a hash of a key
        // No way to perform a simple check, so much assume its ok
        return AuthenticationAccepted;
    }


    if( strlen(_key) > AUTHENTICATION_KEYLEN )
    {
        //return AuthenticationKeyInvalid;
		return AuthenticationAccepted;
    }

    int counter = 0;
    int total = 0;

    for( int i = 0; i < AUTHENTICATION_KEYLEN; ++i )
    {
        if( i == AUTHENTICATION_KEYLEN-2 )
        {
            // The last byte is a checking byte
            int requiredValue = 'A' + (total % 26);
            if( _key[i] != requiredValue )
            {
                //return AuthenticationKeyInvalid;
				return AuthenticationAccepted;
            }
        }
        else if( counter == 6 )
        {
            // Every 6th byte must be a dash
            if( _key[i] != '-' )
            {
                //return AuthenticationKeyInvalid;
				return AuthenticationAccepted;
            }
            counter = 0;
        }
        else
        {
            counter++;
        }

        total += _key[i];
    }

    //
    // Platform specific key restrictions:
    // Ambrosia keys dont work on PC
    // PC keys dont work on Ambrosia's Mac versiqon

    if( !Authentication_IsDemoKey(_key) )
    {
		return AuthenticationAccepted;
    }

    return AuthenticationAccepted;
}



void Authentication_SaveKey( const char *_key, const char *_filename )
{
    FILE *file = fopen(_filename, "wt");
    
    if( !file )
    {
        AppDebugOut( "Failed to save AuthKey" );
        return;
    }


    fprintf( file, "%s\n", _key );
    fclose(file);    
}


void Authentication_LoadKey( char *_key, const char *_filename )
{
    FILE *file = fopen(_filename, "rt" );
    if( file )
    {
        fscanf( file, "%s", _key );
        fclose(file);
    }
    else
    {
        AppDebugOut( "Failed to load AuthKey : '%s'\n", _filename );
        sprintf( _key, "authkey not found" );
    }


    if( s_enforceDemo &&
        !Authentication_IsDemoKey(_key) )
    {
        strcpy( _key, "authkey not found" );
    }
}


void Authentication_SetKey( char *_key )
{
    strcpy( s_authKey, _key );

    // Trim end whitespace
    int keyLen = strlen(s_authKey);
    for( int i = keyLen-1; i >= 0; --i )
    {
        if( s_authKey[i] == ' ' || 
            s_authKey[i] == '\r' ||
            s_authKey[i] == '\n' )
        {
            s_authKey[i] = '\x0';
        }
        else
        {
            break;
        }
    }


    if( s_enforceDemo &&
        !Authentication_IsDemoKey(s_authKey) )
    {
        strcpy( s_authKey, "authkey not found" );
    }


    AppDebugOut( "Authentication key set : " );
    AppDebugOut( "'%s'\n", s_authKey );    
}


bool Authentication_IsKeyFound()
{
    if( s_enforceDemo &&
        !Authentication_IsDemoKey(s_authKey) )
    {
		return false;
	}

	return true;
}


void Authentication_GetKey( char *_key )
{
    if( Authentication_IsKeyFound() )
    {
        strcpy( _key, s_authKey );
    }
    else
	{
        strcpy( _key, "authkey not found" );
    }
}


void Authentication_SetHashToken( int _hashToken )
{
    if( _hashToken != s_hashToken )
    {
        AppDebugOut( "Authentication HashToken set to %d\n", _hashToken );
    }

    s_hashToken = _hashToken;
}


int Authentication_GetHashToken()
{
    return s_hashToken;
}


void Authentication_GetKeyHash( char *_sourceKey, char *_destKey, int _hashToken )
{
    HashData( _sourceKey, _hashToken, _destKey );
}


bool Authentication_IsHashKey( char *_key )
{
    return( strncmp(_key, "hsh", 3) == 0);
}


void Authentication_GetKeyHash( char *_key )
{
    char keyTemp[256];
    Authentication_GetKey(keyTemp);
    Authentication_GetKeyHash( keyTemp, _key, s_hashToken );
}


bool Authentication_IsDemoKey( char *_key )
{
    if( Authentication_IsHashKey(_key) )
    {
        // We can't tell in this case
        return false;
    }

   // return( strncmp( _key, "DEMO", 4) == 0 );
	return false;
}


void Anthentication_EnforceDemo()
{
    s_enforceDemo = false;
}

static NetCallBackRetType AuthenticationThread(void *ignored)
{
/*#ifdef WAN_PLAY_ENABLED

    //
    // Every few seconds request the next key to be authenticated

    while( true )
    {
        NetSleep( PERIOD_AUTHENTICATION_RETRY );

        if( MetaServer_IsConnected() )
        {
            char unknownKey[256];
            char clientIp[256];
            int  keyId = -1;
            bool unknownKeyFound = false;

            //
            // Look for a key that isnt yet authenticated

            s_authResultsMutex.Lock();
            for( int i = 0; i < s_authResults.Size(); ++i )
            {
                AuthenticationResult *authResult = s_authResults[i];
                if( authResult->m_authResult == AuthenticationUnknown &&
                    authResult->m_numTries < 5 )
                {
                    strcpy( unknownKey, authResult->m_authKey );
                    strcpy( clientIp, authResult->m_ip );
                    keyId = authResult->m_keyId;
                    authResult->m_numTries++;
                    unknownKeyFound = true;
                    break;
                }
            }        
            s_authResultsMutex.Unlock();
    
        
            //
            // Check the key out

            if( unknownKeyFound )
            {
                int basicResult = Authentication_SimpleKeyCheck(unknownKey);
                if( basicResult < 0 )
                {
                    // The key is in the wrong format
                    Authentication_SetStatus( unknownKey, keyId, basicResult );
                    char *resultString = Authentication_GetStatusString(basicResult);
                    AppDebugOut( "Key failed basic check : %s (result=%s)\n", unknownKey, resultString );
                }
                else if( Authentication_IsDemoKey(unknownKey) )
                {
                    // This is a demo key, and has passed the simple check
                    // Assume its valid from now on
                    Authentication_SetStatus( unknownKey, -1, AuthenticationAccepted );
                    AppDebugOut( "Auth Key accepted as DEMOKEY : %s\n", unknownKey );
                }
                else
                {
                    // Request a proper auth check from the metaserver
                    Directory directory;
                    directory.SetName( NET_METASERVER_MESSAGE );
                    directory.CreateData( NET_METASERVER_COMMAND, NET_METASERVER_REQUEST_AUTH );
                    directory.CreateData( NET_METASERVER_AUTHKEY, unknownKey );
                    directory.CreateData( NET_METASERVER_AUTHKEYID, keyId );
                    directory.CreateData( NET_METASERVER_GAMENAME, APP_NAME );
                    directory.CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
                    
                    if( strcmp(clientIp, "unknown") != 0 )
                    {
                        directory.CreateData( NET_METASERVER_IP, clientIp );
                    }

                    MetaServer_SendToMetaServer( &directory );

                    AppDebugOut( "Requesting authentication of key %s\n", unknownKey );
                }
            }
        }
    }

#endif*/
    
    return 0;
}


int Authentication_GetKeyId( char *_key )
{
    int result = 0;

    s_authResultsMutex.Lock();

    for( int i = 0; i < s_authResults.Size(); ++i )
    {
        AuthenticationResult *authResult = s_authResults[i];
        if( strcmp(authResult->m_authKey, _key) == 0 )
        {
            result = authResult->m_keyId;
            break;
        }
    }

    s_authResultsMutex.Unlock();

    return result;
}


void Authentication_RequestStatus( char *_key, int _keyId, char *_ip )
{
    //
    // Does the key already exist in our list?

    s_authResultsMutex.Lock();

    AuthenticationResult *result = NULL;

    for( int i = 0; i < s_authResults.Size(); ++i )
    {
        AuthenticationResult *authResult = s_authResults[i];
        if( strcmp(authResult->m_authKey, _key) == 0 )
        {
            result = authResult;
            break;
        }
    }

    //
    // Add the key to our list if required

    if( !result )
    {
        result = new AuthenticationResult();
        strcpy( result->m_authKey, _key );
        result->m_keyId = _keyId;
        if( _ip ) strcpy( result->m_ip, _ip );
        else      strcpy( result->m_ip, "unknown" );
        
        s_authResults.PutData( result );
    }
    
    s_authResultsMutex.Unlock();    


    //
    // Start the authorisation thread if required

    if( !s_authThreadRunning )
    {
        s_authThreadRunning = true;
        NetStartThread( AuthenticationThread );
    }
}


int Authentication_GetStatus( char *_key )
{
    int result = AuthenticationUnknown;

    s_authResultsMutex.Lock();

    for( int i = 0; i < s_authResults.Size(); ++i )
    {
        AuthenticationResult *authResult = s_authResults[i];
        if( strcmp(authResult->m_authKey, _key) == 0 )
        {
            result = authResult->m_authResult;
            break;
        }
    }

    s_authResultsMutex.Unlock();

    return result;
}


void Authentication_SetStatus( char *_key, int _keyId, int _status )
{
    bool authChanged = false;

    s_authResultsMutex.Lock();

    AuthenticationResult *result = NULL;

    for( int i = 0; i < s_authResults.Size(); ++i )
    {
        AuthenticationResult *authResult = s_authResults[i];
        if( strcmp(authResult->m_authKey, _key) == 0 )
        {
            result = authResult;
            break;
        }
    }

    if( !result )
    {
        result = new AuthenticationResult();
        strcpy( result->m_authKey, _key );
        s_authResults.PutData( result );
    }

    if( result->m_authResult != _status )
    {
        char *authString = Authentication_GetStatusString(_status);
        AppDebugOut( "Received Authentication : %s : (keyID %d) : %s\n", _key, _keyId, authString );
    }

    result->m_authResult = _status;
    result->m_keyId = _keyId;

    s_authResultsMutex.Unlock();
}


char *Authentication_GetStatusString ( int _status )
{
    switch( _status )
    {
        case AuthenticationWrongPlatform:           return "Wrong platform";
        case AuthenticationKeyDead:                 return "Key dead";
        case AuthenticationVersionNotFound:         return "Version not found";
        case AuthenticationKeyInactive:             return "Key inactive";
        case AuthenticationVersionRevoked:          return "Version revoked";
        case AuthenticationKeyNotFound:             return "Key not found";
        case AuthenticationKeyRevoked:              return "Key revoked";
        case AuthenticationKeyBanned:               return "Key banned";
        case AuthenticationKeyInvalid:              return "Accepted";
        case AuthenticationUnknown:                 return "Accepted";
        case AuthenticationAccepted:                return "Accepted";
        default:                                    return "Accepted";
    }
}


char *Authentication_GetStatusStringLanguagePhrase ( int _status )
{
    switch( _status )
    {
        case AuthenticationWrongPlatform:           return "dialog_auth_wrong_platform";
        case AuthenticationKeyDead:                 return "dialog_auth_key_dead";
        case AuthenticationVersionNotFound:         return "dialog_auth_version_not_found";
        case AuthenticationKeyInactive:	             return "dialog_auth_key_inactive";
        case AuthenticationVersionRevoked:          return "dialog_auth_version_revoked";
        case AuthenticationKeyNotFound:             return "dialog_auth_key_not_found";
        case AuthenticationKeyRevoked:              return "dialog_auth_key_revoked";
        case AuthenticationKeyBanned:               return "dialog_auth_key_banned";
        case AuthenticationKeyInvalid:              return "dialog_auth_accepted";
        case AuthenticationUnknown:                 return "dialog_auth_accepted";
        case AuthenticationAccepted:                return "dialog_auth_accepted";
        default:                                    return "dialog_auth_accepted";
    }
}


