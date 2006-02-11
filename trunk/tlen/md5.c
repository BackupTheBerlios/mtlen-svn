/*

RVP Protocol Plugin for Miranda IM
Copyright (C) 2005 Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "md5.h"
#include "stdio.h"

#define F(x,y,z) (((x)&(y)) | ((~x) & (z)))
#define G(x,y,z) (((x)&(z)) | ((y) & (~z)))
#define H(x,y,z) ((x)^(y)^(z))
#define I(x,y,z) ((y)^((x) | (~z)))

#define ROTATE_LEFT(x,n) (((x) << (n)) | ((x)>> (32-(n))))

#define FF(a,b,c,d,x,s,ac) {\
	(a)+=F((b),(c),(d))+(x)+(unsigned int)(ac);\
	(a)=ROTATE_LEFT((a),(s));\
	(a)+=(b);\
}
#define GG(a,b,c,d,x,s,ac) {\
	(a)+=G((b),(c),(d))+(x)+(unsigned int)(ac);\
	(a)=ROTATE_LEFT((a),(s));\
	(a)+=(b);\
}
#define HH(a,b,c,d,x,s,ac) {\
	(a)+=H((b),(c),(d))+(x)+(unsigned int)(ac);\
	(a)=ROTATE_LEFT((a),(s));\
	(a)+=(b);\
}
#define II(a,b,c,d,x,s,ac) {\
	(a)+=I((b),(c),(d))+(x)+(unsigned int)(ac);\
	(a)=ROTATE_LEFT((a),(s));\
	(a)+=(b);\
}

void md5_init(MD5 *md5) {
	md5->dataLength[0]=md5->dataLength[1]=0;
	md5->bufferPos=0;
	md5->state[0]=0x67452301;
	md5->state[1]=0xefcdab89;
	md5->state[2]=0x98badcfe;
	md5->state[3]=0x10325476;
}

void md5_transform(MD5 *md5, unsigned char *input) {
	unsigned int i, a,b,c,d,x[16];
 	a=md5->state[0];
  	b=md5->state[1];
   	c=md5->state[2];
    d=md5->state[3];
    // byte to dword
    for (i=0;i<16;i++) {
    	int j=i<<2;
    	unsigned int w1,w2,w3,w4;
    	w1=input[j];
    	w2=input[j+1];
    	w3=input[j+2];
    	w4=input[j+3];
    	x[i]=((unsigned int)w1)|(((unsigned int)w2)<<8)|
     		 (((unsigned int)w3)<<16)|(((unsigned int)w4)<<24);
    }
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

 /* Round 1*/
	FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
	FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
	FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  	FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  	FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  	FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  	FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  	FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  	FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
	FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
 	FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  	FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  	FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  	FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  	FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  	FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */
 /* Round 2 */
  	GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  	GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  	GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  	GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  	GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  	GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  	GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  	GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  	GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  	GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  	GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  	GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  	GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  	GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  	GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  	GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
 /* Round 3 */
	HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
	HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
	HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
	HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
	HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
	HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
	HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
	HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
	HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
	HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
	HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
	HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
	HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
	HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
	HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
	HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
 /* Round 4 */
  	II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  	II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  	II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  	II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  	II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  	II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  	II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  	II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  	II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  	II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  	II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  	II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  	II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  	II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  	II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  	II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
	md5->state[0]+=a;
	md5->state[1]+=b;
	md5->state[2]+=c;
	md5->state[3]+=d;
}

void md5_update(MD5 *md5, unsigned char *input, unsigned  int inputLen) {
	unsigned int i, bufferIndex, appendLen;

	md5->dataLength[0]+=inputLen<<3;
	bufferIndex=md5->bufferPos>>3;
	appendLen=64-bufferIndex;
	if (appendLen<=inputLen) {
		for (i=0;i<appendLen;i++) {
			md5->buffer[bufferIndex+i]=*input;
			input++;
 		}
	 	md5_transform(md5, md5->buffer);
		inputLen=inputLen-appendLen;
		for (;inputLen>64;inputLen-=64) {
		 	md5_transform(md5, input);
	 		input+=64;
	 	}
		bufferIndex=0;
		md5->bufferPos=0;
	}
	for (i=0;i<inputLen;i++) {
		md5->buffer[bufferIndex+i]=*input;
		input++;
 	}
	md5->bufferPos+=inputLen<<3;
	md5->bufferPos&=511;
}

void md5_finalize(MD5 *md5) {
	unsigned int i,padLen, bufferIndex;
	unsigned char padding[64];
	unsigned int dataL[2];
	bufferIndex=md5->bufferPos>>3;
	padLen=bufferIndex<56?56-bufferIndex:64+56-bufferIndex;
    // dword to byte
	dataL[0]=md5->dataLength[0];
	dataL[1]=md5->dataLength[1];
	for (i=0;i<padLen;i++) {
		if (i==0) padding[i]=0x80;
		else padding[i]=0x00;
	}
	md5_update(md5, padding,padLen);
 	md5_update(md5, (unsigned char *)dataL, 8);
}

