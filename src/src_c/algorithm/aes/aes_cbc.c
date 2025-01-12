////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  WjCryptLib_AesCbc
//
//  Implementation of AES CBC cipher.
//
//  Depends on: CryptoLib_Aes
//
//  AES CBC is a cipher using AES in Cipher Block Chaining mode. Encryption and decryption must be performed in
//  multiples of the AES block size (128 bits).
//  This implementation works on both little and big endian architectures.
//
//  This is free and unencumbered software released into the public domain - March 2018 waterjuice.org
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  IMPORTS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "aes_cbc.h"
#include "aes.h"
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MACROS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MIN( x, y ) ( ((x)<(y))?(x):(y) )

#define STORE64H( x, y )                                                       \
   { (y)[0] = (uint8_t)(((x)>>56)&255); (y)[1] = (uint8_t)(((x)>>48)&255);     \
     (y)[2] = (uint8_t)(((x)>>40)&255); (y)[3] = (uint8_t)(((x)>>32)&255);     \
     (y)[4] = (uint8_t)(((x)>>24)&255); (y)[5] = (uint8_t)(((x)>>16)&255);     \
     (y)[6] = (uint8_t)(((x)>>8)&255);  (y)[7] = (uint8_t)((x)&255); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  INTERNAL FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  XorAesBlock
//
//  Takes two source blocks (size AES_BLOCK_SIZE) and XORs them together and puts the result in first block
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static
void
    XorAesBlock
    (
        uint8_t*            Block1,          // [in out]
        uint8_t const*      Block2           // [in]
    )
{
    uint32_t    i;

    for( i=0; i<AES_BLOCK_SIZE; i++ )
    {
        Block1[i] ^= Block2[i];
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  PUBLIC FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcInitialise
//
//  Initialises an AesCbcContext with an already initialised AesContext and a IV. This function can quickly be used
//  to change the IV without requiring the more lengthy processes of reinitialising an AES key.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
    AesCbcInitialise
    (
        AesCbcContext*      Context,                // [out]
        AesContext const*   InitialisedAesContext,  // [in]
        uint8_t const       IV [AES_CBC_IV_SIZE]    // [in]
    )
{
    // Setup context values
    Context->Aes = *InitialisedAesContext;
    memcpy( Context->PreviousCipherBlock, IV, sizeof(Context->PreviousCipherBlock) );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcInitialiseWithKey
//
//  Initialises an AesCbcContext with an AES Key and an IV. This combines the initialising an AES Context and then
//  running AesCbcInitialise. KeySize must be 16, 24, or 32 (for 128, 192, or 256 bit key size)
//  Returns 0 if successful, or -1 if invalid KeySize provided
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int
    AesCbcInitialiseWithKey
    (
        AesCbcContext*      Context,                // [out]
        uint8_t const*      Key,                    // [in]
        uint32_t            KeySize,                // [in]
        uint8_t const       IV [AES_CBC_IV_SIZE]    // [in]
    )
{
    AesContext aes;

    // Initialise AES Context
    if( 0 != AesInitialise( &aes, Key, KeySize ) )
    {
        return -1;
    }

    // Now set-up AesCbcContext
    AesCbcInitialise( Context, &aes, IV );
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcEncrypt
//
//  Encrypts a buffer of data using an AES CBC context. The data buffer must be a multiple of 16 bytes (128 bits)
//  in size. The "position" of the context will be advanced by the buffer amount. A buffer can be encrypted in one
//  go or in smaller chunks at a time. The result will be the same as long as data is fed into the function in the
//  same order.
//  InBuffer and OutBuffer can point to the same location for in-place encrypting.
//  Returns 0 if successful, or -1 if Size is not a multiple of 16 bytes.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int
    AesCbcEncrypt
    (
        AesCbcContext*      Context,                // [in out]
        void const*         InBuffer,               // [in]
        void*               OutBuffer,              // [out]
        uint32_t            Size                    // [in]
    )
{
    uint32_t    numBlocks = Size / AES_BLOCK_SIZE;
    uint32_t    offset = 0;
    uint32_t    i;

    if( 0 != Size % AES_BLOCK_SIZE )
    {
        // Size not a multiple of AES block size (16 bytes).
        return -1;
    }

    for( i=0; i<numBlocks; i++ )
    {
        // XOR on the next block of data onto the previous cipher block
        XorAesBlock( Context->PreviousCipherBlock, (uint8_t*)InBuffer + offset );

        // Encrypt to make new cipher block
        AesEncryptInPlace( &Context->Aes, Context->PreviousCipherBlock );

        // Output cipher block
        memcpy( (uint8_t*)OutBuffer + offset, Context->PreviousCipherBlock, AES_BLOCK_SIZE );

        offset += AES_BLOCK_SIZE;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcDecrypt
//
//  Decrypts a buffer of data using an AES CBC context. The data buffer must be a multiple of 16 bytes (128 bits)
//  in size. The "position" of the context will be advanced by the buffer amount.
//  InBuffer and OutBuffer can point to the same location for in-place decrypting.
//  Returns 0 if successful, or -1 if Size is not a multiple of 16 bytes.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int
    AesCbcDecrypt
    (
        AesCbcContext*      Context,                // [in out]
        void const*         InBuffer,               // [in]
        void*               OutBuffer,              // [out]
        uint32_t            Size                    // [in]
    )
{
    uint32_t    numBlocks = Size / AES_BLOCK_SIZE;
    uint32_t    offset = 0;
    uint32_t    i;
    uint8_t     previousCipherBlock [AES_BLOCK_SIZE];

    if( 0 != Size % AES_BLOCK_SIZE )
    {
        // Size not a multiple of AES block size (16 bytes).
        return -1;
    }

    for( i=0; i<numBlocks; i++ )
    {
        // Copy previous cipher block and place current one in context
        memcpy( previousCipherBlock, Context->PreviousCipherBlock, AES_BLOCK_SIZE );
        memcpy( Context->PreviousCipherBlock, (uint8_t*)InBuffer + offset, AES_BLOCK_SIZE );

        // Decrypt cipher block
        AesDecrypt( &Context->Aes, Context->PreviousCipherBlock, (uint8_t*)OutBuffer + offset );

        // XOR on previous cipher block
        XorAesBlock( (uint8_t*)OutBuffer + offset, previousCipherBlock );

        offset += AES_BLOCK_SIZE;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcEncryptWithKey
//
//  This function combines AesCbcInitialiseWithKey and AesCbcEncrypt. This is suitable when encrypting data in one go
//  with a key that is not going to be reused.
//  InBuffer and OutBuffer can point to the same location for inplace encrypting.
//  Returns 0 if successful, or -1 if invalid KeySize provided or BufferSize not a multiple of 16 bytes.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int
    AesCbcEncryptWithKey
    (
        uint8_t const*      Key,                    // [in]
        uint32_t            KeySize,                // [in]
        uint8_t const       IV [AES_CBC_IV_SIZE],   // [in]
        void const*         InBuffer,               // [in]
        void*               OutBuffer,              // [out]
        uint32_t            BufferSize              // [in]
    )
{
    int             error;
    AesCbcContext   context;

    error = AesCbcInitialiseWithKey( &context, Key, KeySize, IV );
    if( 0 == error )
    {
        error = AesCbcEncrypt( &context, InBuffer, OutBuffer, BufferSize );
    }

    return error;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AesCbcDecryptWithKey
//
//  This function combines AesCbcInitialiseWithKey and AesCbcDecrypt. This is suitable when decrypting data in one go
//  with a key that is not going to be reused.
//  InBuffer and OutBuffer can point to the same location for inplace decrypting.
//  Returns 0 if successful, or -1 if invalid KeySize provided or BufferSize not a multiple of 16 bytes.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int
    AesCbcDecryptWithKey
    (
        uint8_t const*      Key,                    // [in]
        uint32_t            KeySize,                // [in]
        uint8_t const       IV [AES_CBC_IV_SIZE],   // [in]
        void const*         InBuffer,               // [in]
        void*               OutBuffer,              // [out]
        uint32_t            BufferSize              // [in]
    )
{
    int             error;
    AesCbcContext   context;

    error = AesCbcInitialiseWithKey( &context, Key, KeySize, IV );
    if( 0 == error )
    {
        error = AesCbcDecrypt( &context, InBuffer, OutBuffer, BufferSize );
    }

    return error;
}
#if 0
uint8_t *AES_CBC_Pkcs5Padding_Encrypt(uint8_t *data, uint32_t data_len, 
										uint8_t *key, uint32_t key_len, uint8_t *iv, uint32_t iv_len,
										uint32_t *outLen)
{
  	uint32_t blockCount = (data_len / AES_BLOCK_SIZE + 1);
  	uint8_t *in = NULL;
  	uint8_t *out = NULL;
  	uint32_t ret = 0;
  	uint32_t out_len = 0;

	 if(iv_len != AES_BLOCK_SIZE )
		return NULL;
	  
  	in = aes_pkcs5padding(data, data_len, &out_len);
  	if(in == NULL)
  	{
	  	return NULL;
  	}

  	//out
  	out = (uint8_t *)malloc(AES_BLOCK_SIZE*blockCount + 1);
  	if(out == NULL)
  	{
	  	free(in);
	  	return NULL;
  	}
  	memset(out, 0x00, AES_BLOCK_SIZE*blockCount);

  	ret=AesCbcEncryptWithKey(key,key_len,iv,in,out,AES_BLOCK_SIZE*blockCount);
	if(ret)
  	{
	  	free(in);
	  	return NULL;
  	}
  
  	*outLen = AES_BLOCK_SIZE*blockCount;
  	free(in);
  	return out;
}
  
  uint8_t *AES_CBC_Pkcs5Padding_Decrypt(uint8_t *data, uint32_t data_len,
										uint8_t *key, uint32_t key_len,uint8_t *iv, uint32_t iv_len,
										uint32_t *outLen)
  {
	  uint8_t *in = NULL;
	  uint8_t *out = NULL;
	  uint32_t ret = 0;
	 
	  if((iv_len != AES_BLOCK_SIZE) || (data_len % AES_BLOCK_SIZE) )
		return NULL;
	  
	  in = (uint8_t *)malloc(data_len + 1);
	  if(in == NULL)
	  {
		  return NULL;
	  }
	  
	  memset(in,0,data_len);
  
	  ret = AesCbcDecryptWithKey(key,key_len,iv,data,in,data_len);
	  if(ret != 0)
	  {
		  free(in);
		  return NULL;
	  }
	  
	  out = aes_pkcs5unpadding(in, data_len, outLen);
	  if(out == NULL)
	  {
		  free(in);
		  return NULL;
	  }
  
	  free(in);
	  return out;
  }
#else
uint8_t *AES_CBC_Pkcs5Padding_Encrypt(uint8_t *data, uint32_t data_len, 
										uint8_t *key, uint32_t key_len, uint8_t *iv, uint32_t iv_len,
										uint32_t *outLen)
{
	uint32_t blockCount = (data_len / AES_BLOCK_SIZE + 1);
	uint8_t *in = NULL;
	uint8_t *out = NULL;
	uint8_t temp_iv[AES_BLOCK_SIZE] = {0};
	uint8_t temp_data[AES_BLOCK_SIZE] = {0};
	uint32_t i = 0, ret = 0, j=0;
	AesContext	context;
	uint32_t out_len = 0;

	if(iv_len != AES_BLOCK_SIZE )
		return NULL;
	memcpy(temp_iv, iv, AES_BLOCK_SIZE);
	
	in = aes_pkcs5padding(data, data_len, &out_len);
	if(in == NULL)
	{
		return NULL;
	}

	//out
	out = (uint8_t *)malloc(AES_BLOCK_SIZE*blockCount + iv_len +1);
	if(out == NULL)
	{
		free(in);
		return NULL;
	}
	memset(out, 0x00, AES_BLOCK_SIZE*blockCount + iv_len);
	memcpy(out, iv, iv_len);
	
	ret = AesInitialise( &context, key, key_len );
	if(ret != 0)
	{
		free(in);
		free(out);
		return NULL;
	}
	for ( i = 0; i < blockCount; ++i) 
	{
		memcpy(temp_data, (in + i*AES_BLOCK_SIZE), AES_BLOCK_SIZE);
		for(j=0; j<AES_BLOCK_SIZE; ++j)
		{
			temp_data[j] = temp_data[j] ^ temp_iv[j];
		}
		AesEncrypt( &context, temp_data, (out+ iv_len+i*AES_BLOCK_SIZE));
		memcpy(temp_iv, (out+ iv_len+i*AES_BLOCK_SIZE), AES_BLOCK_SIZE);
	}

	*outLen = AES_BLOCK_SIZE*blockCount + iv_len;
	free(in);
	return out;
}

  
uint8_t *AES_CBC_Pkcs5Padding_Decrypt(uint8_t *data, uint32_t data_len,
										uint8_t *key, uint32_t key_len,uint8_t *iv, uint32_t iv_len,
										uint32_t *outLen)
{
	uint8_t *in = NULL;
	uint8_t *out = NULL;
	uint8_t temp_iv[AES_BLOCK_SIZE] = {0};
	uint8_t temp_data[AES_BLOCK_SIZE] = {0};
	uint32_t blockCount = (data_len / AES_BLOCK_SIZE);
	uint32_t i = 0, ret = 0, j=0;
	AesContext	context;

	if((iv_len != AES_BLOCK_SIZE) || (data_len % AES_BLOCK_SIZE) )
		return NULL;
	memcpy(temp_iv, iv, AES_BLOCK_SIZE);
	 
	in = (uint8_t *)malloc(data_len + 1);
	if(in == NULL)
	{
		return NULL;
	}

	memset(in,0,data_len);

	if (blockCount<=0)
	{
		blockCount=1;
	}

	ret = AesInitialise( &context, key, key_len );
	if(ret != 0)
	{
		free(in);
		return NULL;
	}
	for ( i = 0; i < blockCount; ++i) 
	{
		AesDecrypt(&context, (data+i*AES_BLOCK_SIZE), temp_data);
		for(j=0; j<AES_BLOCK_SIZE; ++j)
		{
			temp_data[j] = temp_data[j] ^ temp_iv[j];
		}
		memcpy((in+i*AES_BLOCK_SIZE), temp_data, AES_BLOCK_SIZE);
		memcpy(temp_iv, (data+i*AES_BLOCK_SIZE), AES_BLOCK_SIZE);
	}

	out = aes_pkcs5unpadding(in, data_len, outLen);
	if(out == NULL)
	{
		free(in);
		return NULL;
	}

	free(in);
	return out;
}


#endif
	
