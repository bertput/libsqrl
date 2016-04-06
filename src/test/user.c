/* user.c 

@author Adam Comley

This file is part of libsqrl.  It is released under the MIT license.
For more details, see the LICENSE file included with this package.
**/

#include <unistd.h>
#include <sodium.h>

#include "../sqrl_internal.h"

static int assertions_passed = 0;
#define CHAR_PER_LINE 72

char myPassword[] = "the password";
size_t myPasswordLength = 12;
char myRescueCode[SQRL_RESCUE_CODE_LENGTH+1];

#define ASSERT(m,a) if((a)) { assertions_passed++; printf( "  PASS: %s\n", m); } else { printf( "  FAIL: %s\n", m ); goto ERROR; }

bool onAuthenticationRequired(
	Sqrl_Client_Transaction *transaction,
	Sqrl_Credential_Type credentialType )
{
	char *cred = NULL;
	uint8_t len = 0;
	switch( credentialType ) {
	case SQRL_CREDENTIAL_PASSWORD:
		printf( "   REQ: Password\n" );
		cred = malloc( myPasswordLength + 1 );
		strcpy( cred, myPassword );
		break;
	case SQRL_CREDENTIAL_RESCUE_CODE:
		printf( "   REQ: Rescue Code\n" );
		cred = malloc( SQRL_RESCUE_CODE_LENGTH + 1 );
		strcpy( cred, myRescueCode );
		break;
	case SQRL_CREDENTIAL_HINT:
		printf( "   REQ: Hint\n" );
		len = sqrl_user_get_hint_length( transaction->user );
		cred = malloc( len + 1 );
		strncpy( cred, myPassword, len );
		break;
	default:
		return false;
	}
	sqrl_client_authenticate( transaction, credentialType, cred, strlen( cred ));
	if( cred ) {
		free( cred );
	}
	return true;
}

char transactionType[11][10] = {
	"UNKNWN",
	"IDENT",
	"DISABL",
	"ENABLE",
	"REMOVE",
	"SAVE",
	"RECOVR",
	"REKEY",
	"UNLOCK",
	"LOCK",
	"LOAD"
};
bool showingProgress = false;
int nextProgress = 0;
int onProgress( Sqrl_Client_Transaction *transaction, int p )
{
	if( !showingProgress ) {
		// Transaction type
		showingProgress = true;
		nextProgress = 2;
		printf( "%6s: ", transactionType[transaction->type] );
	}
	const char sym[] = "|****";
	while( p >= nextProgress ) {
		if( nextProgress != 100 ) {
			printf( "%c", sym[nextProgress%5] );
		}
		nextProgress += 2;
	}
	if( p >= 100 ) {
		printf( "\n" );
		showingProgress = false;
	}
	fflush( stdout );
	return 1;

}

void printKV( char *key, char *value ) {
	printf( "%6s: %s\n", key, value );
}

int main() 
{
	bool bError = false;
	sqrl_init();
	char *buf;
	int i;

	Sqrl_Client_Callbacks cbs;
	memset( &cbs, 0, sizeof( Sqrl_Client_Callbacks ));
	cbs.onAuthenticationRequired = onAuthenticationRequired;
	cbs.onProgress = onProgress;
	sqrl_client_set_callbacks( &cbs );

	Sqrl_User *user = sqrl_user_create();

	printf( "    PW: %s\n", myPassword );
	uint8_t saved[SQRL_KEY_SIZE*7];
	uint8_t loaded[SQRL_KEY_SIZE*7];
	uint8_t *sPointer = saved;
	uint8_t *key;

	char str[128];
	for( i = 4; i > 0; i-- ) {
		sqrl_user_rekey( user );
		key = sqrl_user_key( user, KEY_IUK );
		memcpy( sPointer, key, SQRL_KEY_SIZE );
		sPointer += SQRL_KEY_SIZE;
		sodium_bin2hex( str, 128, key, SQRL_KEY_SIZE );
		printKV( "PIUK", str );
	}

	sqrl_user_rekey( user );
	key = sqrl_user_key( user, KEY_IUK );
	memcpy( sPointer, key, SQRL_KEY_SIZE );
	sPointer += SQRL_KEY_SIZE;
	sodium_bin2hex( str, 128, key, SQRL_KEY_SIZE );
	printKV( "IUK", str );
	key = sqrl_user_key( user, KEY_ILK );
	memcpy( sPointer, key, SQRL_KEY_SIZE );
	sPointer += SQRL_KEY_SIZE;
	sodium_bin2hex( str, 128, key, SQRL_KEY_SIZE );
	printKV( "ILK", str );
	key = sqrl_user_key( user, KEY_MK );
	memcpy( sPointer, key, SQRL_KEY_SIZE );
	sodium_bin2hex( str, 128, key, SQRL_KEY_SIZE );
	printKV( "MK", str );
	strcpy( myRescueCode, sqrl_user_get_rescue_code( user ));
	printKV( "RC", str );

	buf = sqrl_user_save_to_buffer( user, NULL, SQRL_EXPORT_ALL, SQRL_ENCODING_BASE64 );
	ASSERT( "export_len", strlen( buf ) == 470 )

	user = sqrl_user_release( user );
	user = sqrl_user_create_from_buffer( buf, strlen(buf) );

	key = sqrl_user_key( user, KEY_MK );
	ASSERT( "load_mk", 0 == sodium_memcmp( key, saved + (SQRL_KEY_SIZE * 6), SQRL_KEY_SIZE ));

	ASSERT( "hintlock_1", !sqrl_user_is_hintlocked( user ) )
	sqrl_user_hintlock( user );
	ASSERT( "hintlock_2", sqrl_user_is_hintlocked( user ) )
	Sqrl_Client_Transaction trans;
	memset( &trans, 0, sizeof( Sqrl_Client_Transaction ));
	trans.user = user;
	sqrl_user_hintunlock( &trans, NULL, 0 );
	ASSERT( "hintlock_3", !sqrl_user_is_hintlocked( user ) )

	key = sqrl_user_key( user, KEY_ILK );
	ASSERT( "load_ilk", 0 == sodium_memcmp( key, saved + (SQRL_KEY_SIZE * 5), SQRL_KEY_SIZE ));
	key = sqrl_user_key( user, KEY_PIUK0 );
	ASSERT( "load_piuk1", 0 == sodium_memcmp( key, saved + (SQRL_KEY_SIZE * 3), SQRL_KEY_SIZE ));
	key = sqrl_user_key( user, KEY_PIUK1 );
	ASSERT( "load_piuk2", 0 == sodium_memcmp( key, saved + (SQRL_KEY_SIZE * 2), SQRL_KEY_SIZE ));
	key = sqrl_user_key( user, KEY_PIUK2 );
	ASSERT( "load_piuk3", 0 == sodium_memcmp( key, saved + (SQRL_KEY_SIZE * 1), SQRL_KEY_SIZE ));
	key = sqrl_user_key( user, KEY_PIUK3 );
	ASSERT( "load_piuk4", 0 == sodium_memcmp( key, saved, SQRL_KEY_SIZE ));

	user = sqrl_user_release( user );
	user = sqrl_user_create_from_buffer( buf, strlen( buf ));
	sPointer = loaded;
	int keys[] = { KEY_PIUK3, KEY_PIUK2, KEY_PIUK1, KEY_PIUK0, KEY_IUK, KEY_ILK, KEY_MK };
	char names[][6] = { "PIUK4", "PIUK3", "PIUK2", "PIUK1", "  IUK", "  ILK", "   MK" };
	for( i = 0; i < 7; i++ ) {
		key = sqrl_user_key( user, keys[i] );
		memcpy( sPointer, key, SQRL_KEY_SIZE );
		sPointer += SQRL_KEY_SIZE;
	}
	ASSERT( "load_rc", 0 == sodium_memcmp( loaded, saved, SQRL_KEY_SIZE * 7 ));

	char *start = buf;
	char *line = buf;
	char tmp[CHAR_PER_LINE + 1];
	size_t total = strlen( buf );
	printf( "  DATA:\n" );
	while( (line - start) < total ) {
		strncpy( tmp, line, CHAR_PER_LINE );
		printf( "%s\n", tmp );
		line += CHAR_PER_LINE;
	}

	free( buf );

	goto DONE;

ERROR:
	bError = true;

DONE:
	//pool = sqrl_entropy_destroy( pool );
	user = sqrl_user_release( user );
	printf( "\nPASSED %d tests.\n", assertions_passed );
	if( bError ) {
		printf( "\nFAILED test %d\n", assertions_passed + 1 );
		exit(1);
	}
	exit(0);
}
