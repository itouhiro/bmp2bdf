/*
 * bmp2bdf  --  supplement tool for bdf2bmp
 * version: 0.1
 * date:    Sun Jan 28 15:25:53 2001
 * author:  ITOU Hiroki (itouh@lycos.ne.jp)
 */

/*
 * Copyright (c) 2001 ITOU Hiroki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stdio.h>  /* printf(), fopen(), fwrite() */
#include <stdlib.h> /* malloc(), EXIT_SUCCESS, strtol(), exit() */
#include <string.h> /* strncmp(), strcpy() */
#include <limits.h> /* strtol() */
#include <sys/stat.h> /* stat() */
#include <sys/types.h>
#include <ctype.h> /* isspace() */

#define FILENAMECHARMAX 256
#define LINECHARMAX 1024
#define CLUTNOTBLACK 4
#define SIZEMAX 8
enum { LSB, MSB };
#define exiterror(msg,arg) {printf("error: " msg, arg); exit(EXIT_FAILURE);}

#ifdef DEBUG
#define d_printf(msg,arg) printf(msg, arg)
#else /* not DEBUG */
#define d_printf(msg,arg)
#endif /* DEBUG */


/* global var */
struct imageinfo{
	long w;
	long h;
	int size;
	int sp; /* width of spacing */
} img;

struct {
	int w; /* width (pixel) */
	int h; /* height */
	int offx; /* offset y (pixel) */
	int offy; /* offset y */
} font;
int numcol; /*	numberOfColumns */
int endian;


/* func prototype */
int main(int argc, char *argv[]);
char *readBmp(char *infileBmpP);
void readwriteBdf(char *infileBdfP, char *imgP, char *outfileBdfP);
int getline(char* lineP, int max, FILE* inputP);
char *gettoken(char *strP);
void newglyphwrite(FILE *outP, int nglyph, char *imgP);
void checkEndian(void);
long mread(const void *varP, int size);



int main(int argc, char *argv[]){
	char *imgP;
	char infileBdfP[FILENAMECHARMAX];
	char infileBmpP[FILENAMECHARMAX];
	char outfileBdfP[FILENAMECHARMAX];

	checkEndian();

	/* arg check */
	if((argc<5) || (argc>5)){
		printf("bmp2bdf 0.1\n");
		printf("Usage: bmp2bdf numberOfColumns input.bdf input.bmp output.bdf\n");
		exit(EXIT_FAILURE);
	}
	numcol = atoi(argv[1]);
	if((numcol<0) || (numcol>1024))
		exiterror("numberOfColumns: %d is over the limit\n", numcol);
	strcpy(infileBdfP, argv[2]);
	strcpy(infileBmpP, argv[3]);
	strcpy(outfileBdfP, argv[4]);

	if(strcmp(infileBdfP, outfileBdfP) == 0)
		exiterror("'%s' appears twice\n", infileBdfP);

	/* read BMP file and change to IMG file */
	imgP = readBmp(infileBmpP);

	/* read and write BDF file*/
	readwriteBdf(infileBdfP, imgP, outfileBdfP);

	return EXIT_SUCCESS;
}


char *readBmp(char *infileBmpP){
	struct stat fileinfo;
	unsigned char *bmpP;
	struct imageinfo bmp;
	int val, i, j, k;
	int nclut; /* number of CLUT(color lookup table) */
	int clutindex;
	FILE *inP;
	char *imgP;
	unsigned char notblack[CLUTNOTBLACK]; /* index number of palette */
	long bmpoffset; /* bfOffBits: offset to image-data array */
	short bmpdepth;
	long bmpwidthaligned;
	unsigned char buf;
	char c;

	/*
	 * open BMP file
	 */
	if(stat(infileBmpP, &fileinfo) != 0)
		exiterror("'%s' does not exist", infileBmpP);
	bmpP = malloc(fileinfo.st_size);
	if(bmpP == NULL)
		exiterror("cannot allocate memory in %s'\n", infileBmpP);

	inP = fopen(infileBmpP, "rb");
	if(inP == NULL)
		exiterror("cannot open '%s'\n", infileBmpP);

	val = fread(bmpP, 1, fileinfo.st_size, inP);
	if(val != fileinfo.st_size)
		exiterror("cannot read '%s'\n", infileBmpP);
	if(memcmp(bmpP, "BM", 2) != 0)
		exiterror("%s' is not a BMP file.\n", infileBmpP);
	fclose(inP);

	/*
	 * analyze BMP file
	 */
	bmpoffset = mread(bmpP+10, sizeof(long));
	bmp.w = img.w = mread(bmpP+18, sizeof(long));
	d_printf("BMP width = %ld ", bmp.w);
	bmp.h = img.h = mread(bmpP+22, sizeof(long));
	d_printf("height = %ld\n", bmp.h);
	if(bmp.h < 0)
		exiterror("bmp height(%ld) is minus-number\n", bmp.h);

	bmpdepth = (short)mread(bmpP+28, sizeof(short));
	if(bmpdepth != 8)
		exiterror("BMP bitdepth(%d) must be 8bits.\n",bmpdepth);
	/* bmp.size = fileinfo.st_size; */

	/* check CLUT; black(RGB=0,0,0) or not */
	/*   '54' means CLUT address */
	for(i=54,nclut=0,clutindex=0; i<bmpoffset; i+=4,clutindex++){
		if( (*(bmpP+i)==0) && (*(bmpP+i+1)==0) && (*(bmpP+i+2)==0)){
			; /* black; do nothing */
		}else{
			/* not black */
			if(nclut > CLUTNOTBLACK)
				exiterror("Not-black CLUT is too many.\n\tThe number must be within %d.\n", CLUTNOTBLACK);
			notblack[nclut] = clutindex;
			d_printf("%02x ", clutindex);
			nclut++;
		}
	}
	d_printf("  Not-black CLUT: %d\n", nclut);

	/*
	 * change BMP format to IMG format
	 *   IMG format(this-src-only format):
	 *		 remove BMP-header,padding
	 *		 reverse down-top to top-down
	 *		 spacing is left
	 */
	bmpwidthaligned = (bmp.w + 3) / 4 * 4; /* BMP width with padding */
	img.size = img.w * img.h;
	imgP = malloc(img.size); /*  free(imageP) does not exist */
	if(imgP == NULL)
		exiterror("cannot allocate memory imageP %dbytes\n", img.size);

	for(i=0; i<img.h; i++){
		for(j=0; j<img.w; j++){
			buf = *(bmpP + bmpoffset + (bmp.h-1-i)*bmpwidthaligned + j);

			/* black? or not? */
			c = '1';
			for(k=0; k<nclut; k++)
				if(buf == notblack[k])
					c = '0'; /* not black */

			*(imgP + i*img.w + j) = c;
		}
	}

	free(bmpP);
	return imgP;
}


void readwriteBdf(char *infileBdfP, char *imgP, char *outfileBdfP){
	FILE *inP;
	FILE *outP;
	enum {INBITMAP, OUTBITMAP};
	struct stat fileinfo;
	char lineP[LINECHARMAX];
	char strP[LINECHARMAX]; /* for not-breaking lineP with '\0' */
	int st; /* status */
	int len, val;
	int nglyph; /* number of glyphs that appeared */
	char *tokenP; /* used for splitting tokens */

	/* file open */
	if(stat(infileBdfP, &fileinfo) != 0)
		exiterror("'%s' does not exist\n", infileBdfP);
	inP = fopen(infileBdfP, "r");
	if(inP == NULL)
		exiterror("cannot open '%s'\n", infileBdfP);

	if(stat(outfileBdfP, &fileinfo) == 0)
		exiterror("'%s' already exists\n", outfileBdfP);
	outP = fopen(outfileBdfP, "wb");
	if(outP == NULL)
		exiterror("cannot write '%s'\n", outfileBdfP);

	/* file check */
	memset(lineP, 0x00, LINECHARMAX);
	len = getline(lineP, LINECHARMAX, inP);
	if((len == 0) || (strncmp(lineP, "STARTFONT ", 10) != 0))
		exiterror("'%s' is not a BDF file.\n", infileBdfP);
	val = fwrite(lineP, 1, strlen(lineP), outP);
	if(val != (int)strlen(lineP))
		exiterror("cannot write '%s'\n", outfileBdfP);


	/* read bdf file */
	nglyph = 0;
	st = OUTBITMAP;
	for(;;){
		memset(lineP, 0x00, LINECHARMAX);
		len = getline(lineP, LINECHARMAX, inP);
		if(len == 0)
			break;

		if(st == INBITMAP){
			if(strncmp(lineP, "ENDCHAR", 7)==0){
				st = OUTBITMAP;
				newglyphwrite(outP, nglyph, imgP);
				nglyph++;
			}
		}else{ /*  stat != INBITMAP */
			switch(lineP[0]){
			case 'B':
				if(strncmp(lineP, "BBX", 3)==0)
					; /*  do nothing */
				else if(strncmp(lineP, "BITMAP", 6)==0)
					st = INBITMAP;
				break; /* lines of BBX, BITMAP is written later */
			case 'F':
				if(strncmp(lineP, "FONTBOUNDINGBOX ", 16)==0){
					strcpy(strP, lineP);
					tokenP = gettoken(strP);
					tokenP = gettoken(tokenP + (int)strlen(tokenP) + 1);
					font.w = atoi(tokenP);
					tokenP = gettoken(tokenP + (int)strlen(tokenP) + 1);
					font.h = atoi(tokenP);
					tokenP = gettoken(tokenP + (int)strlen(tokenP) + 1);
					font.offx = atoi(tokenP);
					tokenP = gettoken(tokenP + (int)strlen(tokenP) + 1);
					font.offy = atoi(tokenP);

					if( (img.w-numcol*font.w) % (numcol+1) != 0){
						d_printf("numcol = %d ", numcol);
						d_printf("numcol*font.h = %d\n", numcol*font.h);

						exiterror("NumberOfColumns(%d) and a BMP file do not match.\n", numcol);
					}
					img.sp = (img.w - numcol*font.w) / (numcol+1);
					d_printf("font.w=%d ", font.w);
					d_printf("font.h=%d ", font.h);
					d_printf("font.offx=%d ", font.offx);
					d_printf("font.offy=%d\n", font.offy);
				}
				/*  go next */
			default:
				val = fwrite(lineP, 1, strlen(lineP), outP);
				if(val != (int)strlen(lineP))
					exiterror("cannot write '%s'\n", outfileBdfP);
			} /* switch */
		} /* if(stat=.. */
	} /* for(;;) */

	fclose(inP);
	fclose(outP);
	d_printf("total glyphs = %d\n", nglyph);
}

/*
 * read oneline from textfile
 *    fgets returns strings included '\n'
 */
int getline(char* lineP, int max, FILE* inputP){
	if (fgets(lineP, max, inputP) == NULL)
		return 0;
	else
		return strlen(lineP);
}


char *gettoken(char *strP){
	enum {BEFORE, AFTER};
	char *retP = strP;
	int st; /* status */

	st = BEFORE;
	while(*strP != '\0'){
		if((st==BEFORE) && (isspace(*strP)==0)){
			/* before first char of token, not space */
			retP = strP;
			st = AFTER;
		}else if((st==AFTER) && (isspace(*strP)!=0)){
			/* after first char of token, space */
			*strP = '\0';
			return retP;
		}
		strP++;
	}
	/* return NULL; */
	return retP;
}

/*
 *  read glyphimage from IMG
 *     and change to BDF hexadecimal
 */
void newglyphwrite(FILE *outP, int nglyph, char *imgP){
	char *boxP;
	int i,j,k, x,y, val;
	int widthaligned; /* width of a glyph (byte-aligned) */
	char *bitstrP; /* BITMAP strings */
	char strP[LINECHARMAX]; /* tmp buffer */
	static char *hex2binP[]= {
		"0000","0001","0010","0011","0100","0101","0110","0111",
		"1000","1001","1010","1011","1100","1101","1110","1111"
	};

	/*
	 * allocate 'box' area for a glyph image
	 */
	widthaligned = (font.w+7) / 8 * 8;
	boxP = malloc(widthaligned*font.h);
	if(boxP == NULL)
		exiterror("cannot allocate memory newglyphwrite %dbytes\n", widthaligned*font.h);
	memset(boxP, '0', widthaligned*font.h);

	/*
	 * read from IMG
	 */
	/*   one glyph height  row */
	y = (font.h+img.sp) * (nglyph/numcol) + img.sp;
	/*   one glyph width   column */
	x = (font.w+img.sp)*(nglyph%numcol) + img.sp;

	for(i=0; i<font.h; i++)
		for(j=0; j<font.w; j++)
			*(boxP + i*widthaligned + j) = *(imgP + (y+i)*img.w + x+j);

	/*
	 * change to hexadecimal
	 */
	bitstrP = malloc(LINECHARMAX * font.h);
	if(bitstrP == NULL)
		exiterror("cannot allocate memory: bitstrP %dbytes\n",LINECHARMAX*font.h);
	memset(bitstrP, 0x00, LINECHARMAX*font.h);

	/* write BBX , BITMAP to tmpVariable */
	sprintf(bitstrP, "BBX %d %d %d %d\nBITMAP\n",
		font.w, font.h, font.offx, font.offy);

	/* write bitmapData to tmpVariable */
	for(i=0; i<font.h; i++){
		for(j=0; j<widthaligned; j+=4){
			for(k=0; k<16; k++){
				if(strncmp(boxP+i*widthaligned+j, hex2binP[k],4)==0)
					sprintf(strP, "%x", k);
			}
			strcat(bitstrP, strP);
		}
		strcat(bitstrP, "\n");
	}
	strcat(bitstrP, "ENDCHAR\n");

	/* write tmpVariable to disk */
	val = fwrite(bitstrP, 1, strlen(bitstrP), outP);
	if(val != (int)strlen(bitstrP))
		exiterror("cannot write a file: bitstrP %dbytes", (int)strlen(bitstrP));

	free(boxP);
	free(bitstrP);
	return;
}


/*
 * your-CPU byte-order is MSB or LSB?
 * MSB .. Most Significant Byte first (BigEndian)  e.g. PowerPC, SPARC
 * LSB .. Least Significant Byte first (LittleEndian) e.g. Intel Pentium
 */
void checkEndian(void){
	unsigned long ulong = 0x12345678;
	unsigned char *ucharP;

	ucharP = (unsigned char *)(&ulong);
	if(*ucharP == 0x78){
		d_printf("LSB 0x%x\n", *ucharP);
		endian = LSB;
	}else{
		d_printf("MSB 0x%x\n", *ucharP);
		endian = MSB;
	}
}


/*
 * read memory
 */
long mread(const void *varP, int size){
	const unsigned char *ucharP = varP;
	char strP[LINECHARMAX];
	char tmpP[SIZEMAX];
	int i;

	strcpy(strP, "0x");

	if(endian == LSB){
		for(i=size-1; i>=0; i--){
			sprintf(tmpP, "%x", *(ucharP+i));
			strcat(strP, tmpP);
		}
	}else{
		for(i=0; i<size; i++){
			sprintf(tmpP, "%x", *(ucharP+i));
			strcat(strP, tmpP);
		}
	}

	return strtol(strP,(char **)NULL, 16);
}
