#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "../libs/win32/include/sodium.h"

typedef void ( *PrintStream )( const char *format, ... );

// most functions return these codes depending on result
#define CRYPTO_FINE				0
#define CRYPTO_ERROR			-1

#define HexSizeForBin( bin )	( bin * 2 + 1 )

// general sizes for cipher buffers
#define CRYPTO_CIPHER_RAW_CHARS	( 127 - crypto_box_SEALBYTES )						// max chars of a raw unencrypted string, NULL excluded
#define CRYPTO_CIPHER_RAW_SIZE	( CRYPTO_CIPHER_RAW_CHARS + 1 )						// same as above, but NULL included, so use this one for buffers
#define CRYPTO_CIPHER_BIN_SIZE	( crypto_box_SEALBYTES + CRYPTO_CIPHER_RAW_CHARS )	// constant size of the binary encrypted message
#define CRYPTO_CIPHER_HEX_SIZE	HexSizeForBin( CRYPTO_CIPHER_BIN_SIZE )				// constant chars of the hex encoded encrypted message, NULL included

#if CRYPTO_CIPHER_HEX_SIZE > 256 // MAX_CVAR_VALUE_STRING
#error CRYPTO_CIPHERHEX_LEN does not fit inside a cvar (max 255 chars)
#endif

#define CRYPTO_PUBKEY_BIN_SIZE	crypto_box_PUBLICKEYBYTES
#define CRYPTO_PUBKEY_HEX_SIZE	HexSizeForBin( CRYPTO_PUBKEY_BIN_SIZE ) // NULL included

typedef struct publicKey_s {
	unsigned char	keyBin[CRYPTO_PUBKEY_BIN_SIZE];
	char			keyHex[CRYPTO_PUBKEY_HEX_SIZE];
} publicKey_t;

#define CRYPTO_SECKEY_BIN_SIZE	crypto_box_SECRETKEYBYTES
#define CRYPTO_SECKEY_HEX_SIZE	HexSizeForBin( CRYPTO_SECKEY_BIN_SIZE ) // NULL included

typedef struct secretKey_s {
	unsigned char	keyBin[CRYPTO_SECKEY_BIN_SIZE];
	char			keyHex[CRYPTO_SECKEY_HEX_SIZE];
} secretKey_t;

// hash buffer sizes
#define CRYPTO_HASH_BIN_SIZE	20
#define CRYPTO_HASH_HEX_SIZE	HexSizeForBin( CRYPTO_HASH_BIN_SIZE ) // NULL included

#if CRYPTO_HASH_BIN_SIZE < crypto_generichash_BYTES_MIN 
#error CRYPTO_HASH_BIN_LEN is too small
#endif

#if CRYPTO_HASH_BIN_SIZE > crypto_generichash_BYTES_MAX
#error CRYPTO_HASH_BIN_LEN is too large
#endif

int Crypto_Init( PrintStream errorStream );
int Crypto_GenerateKeys( publicKey_t *pk, secretKey_t *sk );
int Crypto_LoadKeysFromStrings( publicKey_t *pk, const char *pkHex, secretKey_t *sk, const char *skHex );
int Crypto_LoadKeysFromFiles( publicKey_t *pk, const char *pkFilename, secretKey_t *sk, const char *skFilename );
int Crypto_SaveKeysToFiles( publicKey_t *pk, const char *pkFilename, secretKey_t *sk, const char *skFilename );
int Crypto_Encrypt( publicKey_t *pk, const char *inRaw, char *outHex, size_t outHexSize );
int Crypto_Decrypt( publicKey_t *pk, secretKey_t *sk, const char *inHex, char *outRaw, size_t outRawSize );
int Crypto_Hash( const char *inRaw, char *outHex, size_t outHexSize );

#endif // __CRYPTO_H__