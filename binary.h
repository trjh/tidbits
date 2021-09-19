/* binary.h -- some quick macros from StackOverflow for printing numbers in
 * binary format
 */

#define BP "%c%c%c%c%c%c%c%c"
#define B(byte)  \
	(byte & 0x80 ? '1' : '0'), \
	(byte & 0x40 ? '1' : '0'), \
	(byte & 0x20 ? '1' : '0'), \
	(byte & 0x10 ? '1' : '0'), \
	(byte & 0x08 ? '1' : '0'), \
	(byte & 0x04 ? '1' : '0'), \
	(byte & 0x02 ? '1' : '0'), \
	(byte & 0x01 ? '1' : '0') 

#define LBP "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define LB(byte)  \
	(byte & 0x8000 ? '1' : '0'), \
	(byte & 0x4000 ? '1' : '0'), \
	(byte & 0x2000 ? '1' : '0'), \
	(byte & 0x1000 ? '1' : '0'), \
	(byte &  0x800 ? '1' : '0'), \
	(byte &  0x400 ? '1' : '0'), \
	(byte &  0x200 ? '1' : '0'), \
	(byte &  0x100 ? '1' : '0'), \
	(byte &   0x80 ? '1' : '0'), \
	(byte &   0x40 ? '1' : '0'), \
	(byte &   0x20 ? '1' : '0'), \
	(byte &   0x10 ? '1' : '0'), \
	(byte &   0x08 ? '1' : '0'), \
	(byte &   0x04 ? '1' : '0'), \
	(byte &   0x02 ? '1' : '0'), \
	(byte &   0x01 ? '1' : '0') 

/* hamming weight calculation, from 
   https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
*/
#define NumberOfSetBits(i) { \
     i = i - ((i >> 1) & 0x55555555); \
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333); \
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24; \
}
