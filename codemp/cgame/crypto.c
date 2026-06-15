#include "string.h"
#include "crypto.h"
#include <stdio.h>
#pragma comment (lib, "libsodium.lib")
static inline void strncpyz( const char *src, char *dest, size_t destSize ) {
	strncpy( dest, src, destSize - 1 );
	dest[destSize - 1] = '\0';
}

static inline unsigned long getFileLength( FILE *f ) {
	unsigned long result;

	fseek( f, 0, SEEK_END );
	result = ftell( f );
	fseek( f, 0, SEEK_SET );

	return result;
}

static PrintStream CryptoErrorStream = NULL;

#define Crypto_Error( format, ... )															\
	do {																					\
		if ( CryptoErrorStream ) {															\
			CryptoErrorStream( "(ERROR) %s(): " format "\n", __FUNCTION__, ##__VA_ARGS__ );	\
		}																					\
	}																						\
	while ( 0 )

// Initializes all crypto related functionality.
// Errors will be redirected to the provided stream, or silenced if NULL.
int Crypto_Init( PrintStream errorStream ) {
	CryptoErrorStream = errorStream;

	if ( sodium_init() == -1 ) {
		Crypto_Error( "Failed to initialize libsodium" );
		return CRYPTO_ERROR;
	}

	return CRYPTO_FINE;
}

// Generates a random public/secret key pair.
int Crypto_GenerateKeys( publicKey_t *pk, secretKey_t *sk ) {
	if ( crypto_box_keypair( pk->keyBin, sk->keyBin ) ) {
		Crypto_Error( "Failed to generate public/secret key pair" );
		return CRYPTO_ERROR;
	}

	if ( sodium_bin2hex( pk->keyHex, sizeof( pk->keyHex ), pk->keyBin, sizeof( pk->keyBin ) ) != pk->keyHex ||
		sodium_bin2hex( sk->keyHex, sizeof( sk->keyHex ), sk->keyBin, sizeof( sk->keyBin ) ) != sk->keyHex ) {
		Crypto_Error( "Could not convert the generated binary key pair to hexadecimal" );
		return CRYPTO_ERROR;
	}

	return CRYPTO_FINE;
}

// Loads keys directly using hexadecimal strings. Skip a key by passing NULL for its arguments.
int Crypto_LoadKeysFromStrings( publicKey_t *pk, const char *pkHex, secretKey_t *sk, const char *skHex ) {
	if ( pk && pkHex ) {
		if ( strlen( pkHex ) != CRYPTO_PUBKEY_HEX_SIZE - 1 ) {
			Crypto_Error( "Provided hexadecimal public key has incorrect size: %d != %d chars", strlen( pkHex ), CRYPTO_PUBKEY_HEX_SIZE - 1 );
			return CRYPTO_ERROR;
		}

		if ( sodium_hex2bin( pk->keyBin, sizeof( pk->keyBin ), pkHex, strlen( pkHex ), "", NULL, NULL ) != 0 ) {
			Crypto_Error( "Could not convert the provided hexadecimal public key to binary: %s", pkHex );
			return CRYPTO_ERROR;
		}

		strncpyz( pkHex, pk->keyHex, sizeof( pk->keyHex ) );
	}

	if ( sk && skHex ) {
		if ( strlen( skHex ) != CRYPTO_SECKEY_HEX_SIZE - 1 ) {
			Crypto_Error( "Provided hexadecimal secret key has incorrect size: %d != %d chars", strlen( skHex ), CRYPTO_SECKEY_HEX_SIZE - 1 );
			return CRYPTO_ERROR;
		}

		if ( sodium_hex2bin( sk->keyBin, sizeof( sk->keyBin ), skHex, strlen( skHex ), "", NULL, NULL ) != 0 ) {
			Crypto_Error( "Could not convert the provided hexadecimal secret key to binary: %s", skHex );
			return CRYPTO_ERROR;
		}

		strncpyz( skHex, sk->keyHex, sizeof( sk->keyHex ) );
	}

	return CRYPTO_FINE;
}

// Loads keys directly from binary files. Skip a key by passing NULL for its arguments.
int Crypto_LoadKeysFromFiles( publicKey_t *pk, const char *pkFilename, secretKey_t *sk, const char *skFilename ) {
	if ( pk && pkFilename ) {
		FILE *f = fopen( pkFilename, "rb" );

		if ( !f ) {
			Crypto_Error( "Failed to open binary public key file for reading: %s", pkFilename );
			return CRYPTO_ERROR;
		}

		unsigned long fileLength = getFileLength( f );
		if ( fileLength != CRYPTO_PUBKEY_BIN_SIZE ) {
			Crypto_Error( "Binary public key file has incorrect size: %d != %d bytes", fileLength, CRYPTO_PUBKEY_BIN_SIZE );
			fclose( f );
			return CRYPTO_ERROR;
		}

		fread( pk->keyBin, fileLength, 1, f );
		fclose( f );

		if ( sodium_bin2hex( pk->keyHex, sizeof( pk->keyHex ), pk->keyBin, sizeof( pk->keyBin ) ) != pk->keyHex ) {
			Crypto_Error( "Could not convert the binary public key to hexadecimal" );
			return CRYPTO_ERROR;
		}
	}

	if ( sk && skFilename ) {
		FILE *f = fopen( skFilename, "rb" );

		if ( !f ) {
			Crypto_Error( "Failed to open binary secret key file for reading: %s", skFilename );
			return CRYPTO_ERROR;
		}

		unsigned long fileLength = getFileLength( f );
		if ( fileLength != CRYPTO_SECKEY_BIN_SIZE ) {
			Crypto_Error( "Binary secret key file has incorrect size: %d != %d bytes", fileLength, CRYPTO_SECKEY_BIN_SIZE );
			fclose( f );
			return CRYPTO_ERROR;
		}

		fread( sk->keyBin, fileLength, 1, f );
		fclose( f );

		if ( sodium_bin2hex( sk->keyHex, sizeof( sk->keyHex ), sk->keyBin, sizeof( sk->keyBin ) ) != sk->keyHex ) {
			Crypto_Error( "Could not convert the binary secret key to hexadecimal" );
			return CRYPTO_ERROR;
		}
	}

	return CRYPTO_FINE;
}

// Saves keys to different files. Skip a key by passing NULL for its arguments.
int Crypto_SaveKeysToFiles( publicKey_t *pk, const char *pkFilename, secretKey_t *sk, const char *skFilename ) {
	if ( pk && pkFilename ) {
		FILE *f = fopen( pkFilename, "wb" );

		if ( !f ) {
			Crypto_Error( "Failed to open binary public key file for writing: %s", pkFilename );
			return CRYPTO_ERROR;
		}

		fwrite( pk->keyBin, 1, sizeof( pk->keyBin ), f );
		fclose( f );
	}

	if ( sk && skFilename ) {
		FILE *f = fopen( skFilename, "wb" );

		if ( !f ) {
			Crypto_Error( "Failed to open binary secret key file for writing: %s", skFilename );
			return CRYPTO_ERROR;
		}

		fwrite( sk->keyBin, 1, sizeof( sk->keyBin ), f );
		fclose( f );
	}

	return CRYPTO_FINE;
}

// Encrypts the message and encodes it in hexadecimal.
int Crypto_Encrypt( publicKey_t *pk, const char *inRaw, char *outHex, size_t outHexSize ) {
	if ( strlen( inRaw ) > CRYPTO_CIPHER_RAW_CHARS ) {
		Crypto_Error( "The raw text to encrypt (\"%s\") is too long: %d > %d chars", inRaw, strlen( inRaw ), CRYPTO_CIPHER_RAW_CHARS );
		return CRYPTO_ERROR;
	}

	if ( outHexSize < CRYPTO_CIPHER_HEX_SIZE ) {
		Crypto_Error( "The hexadecimal output buffer is too small: %d < %d bytes", outHexSize, CRYPTO_CIPHER_HEX_SIZE );
		return CRYPTO_ERROR;
	}

	// encrypt the message
	unsigned char cipherBin[CRYPTO_CIPHER_BIN_SIZE];
	if ( crypto_box_seal( cipherBin, ( const unsigned char* )inRaw, CRYPTO_CIPHER_RAW_CHARS, pk->keyBin ) != 0 ) {
		Crypto_Error( "Failed to encrypt the text: \"%s\"", inRaw );
		return CRYPTO_ERROR;
	}

	// encode in hex to output
	if ( sodium_bin2hex( outHex, outHexSize, cipherBin, sizeof( cipherBin ) ) != outHex ) {
		Crypto_Error( "Failed to encode the encrypted text to hexadecimal: \"%s\"", inRaw );
		return CRYPTO_ERROR;
	}

	return CRYPTO_FINE;
}

// Decrypts the hexadecimal encoded message.
int Crypto_Decrypt( publicKey_t *pk, secretKey_t *sk, const char *inHex, char *outRaw, size_t outRawSize ) {
	if ( strlen( inHex ) > CRYPTO_CIPHER_HEX_SIZE - 1 ) {
		Crypto_Error( "The hexadecimal text to decrypt (\"%s\") is too long: %d > %d chars", inHex, strlen( inHex ), CRYPTO_CIPHER_HEX_SIZE - 1 );
		return CRYPTO_ERROR;
	}

	if ( outRawSize < CRYPTO_CIPHER_RAW_SIZE ) {
		Crypto_Error( "The raw output buffer is too small: %d < %d bytes", outRawSize, CRYPTO_CIPHER_RAW_SIZE );
		return CRYPTO_ERROR;
	}

	// decode to binary
	unsigned char cipherBin[CRYPTO_CIPHER_BIN_SIZE];
	if ( sodium_hex2bin( cipherBin, sizeof( cipherBin ), inHex, strlen( inHex ), "", NULL, NULL ) != 0 ) {
		Crypto_Error( "Failed to decode the encrypted hexadecimal text to binary: \"%s\"", inHex );
		return CRYPTO_ERROR;
	}

	// decrypt to output
	if ( crypto_box_seal_open( ( unsigned char* )outRaw, cipherBin, sizeof( cipherBin ), pk->keyBin, sk->keyBin ) != 0 ) {
		Crypto_Error( "Failed to decrypt the hexadecimal text: \"%s\"", inHex );
		return CRYPTO_ERROR;
	}

	return CRYPTO_FINE;
}

// Hashes text of arbitrary length into a hexadecimal output of fixed length.
int Crypto_Hash( const char *inRaw, char *outHex, size_t outHexSize ) {
	if ( outHexSize < CRYPTO_HASH_HEX_SIZE ) {
		Crypto_Error( "The hexadecimal output buffer is too small: %d < %d bytes", outHexSize, CRYPTO_HASH_HEX_SIZE );
		return CRYPTO_ERROR;
	}

	// hash the data
	unsigned char hashBin[CRYPTO_HASH_BIN_SIZE];
	if ( crypto_generichash( hashBin, sizeof( hashBin ), ( const unsigned char* )inRaw, strlen( inRaw ), NULL, 0 ) != 0 ) {
		Crypto_Error( "Failed to hash text: %s", inRaw );
		return CRYPTO_ERROR;
	}

	// encode in hex to output
	if ( sodium_bin2hex( outHex, outHexSize, hashBin, sizeof( hashBin ) ) != outHex ) {
		Crypto_Error( "Failed to encode the hashed text to hexadecimal: \"%s\"", inRaw );
		return CRYPTO_ERROR;
	}

	return CRYPTO_FINE;
}