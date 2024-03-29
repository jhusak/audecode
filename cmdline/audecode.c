#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "tinywav.h"

#define MAX_SECT_NUM 2880

typedef struct {
	int resol;
	int levels; // 256 or 65536
	int next;
	int endian;
	int ssigned;
	float* start_ptr;
	float* curr_ptr;
	uint32_t bytelength;
	uint32_t framelength;
	float edge_threshold;
	float prev_val;
	float curr_val;
} TWaveState;

typedef enum {
	ssEnd,
	ssEmpty,
	ssGood,
	ssBad
} SStatus;

int analyze_buffer[200];
uint8_t chksum;
int MaxSector;
uint8_t xfd_sector_data[256 * MAX_SECT_NUM+16];
SStatus xfd_sector_status[6000];
int xfd_sector_length;
TWaveState wave_state;
char fname[1024];
char savefname[1024];
FILE* wav;
int Eof=0;

int force_save=0;
int verbose=0;
float t_corr=0;
int incomplete=0;

float GetNextSample() {
	//printf("getting sample: %ld: %f\n",wave_state.curr_ptr - wave_state.start_ptr,*(wave_state.curr_ptr));
	float result = -0x10000;
	if ((wave_state.curr_ptr - wave_state.start_ptr) >= wave_state.framelength - 3) {
		return result;
	}
	wave_state.prev_val = wave_state.curr_val;
	result = *(wave_state.curr_ptr);

	wave_state.curr_val = result;
	wave_state.curr_ptr ++;
	return result;
}

float GetNextDiff() {
	float result = GetNextSample();
	if (result <= -0xffff) {
		Eof=1;
		return result;
	}
	result = wave_state.curr_val - wave_state.prev_val;
	if (result < 0) {
		result = -result;
	}
	//printf("getting diff: %f\n",result);
	return result;
}

int GetImpulseLength() {
	int l;
	float val,mx;
	val = GetNextDiff();
	int result = 0;

	if (val < -0xffff) {
		Eof=1;
		return result;
	}
	mx = 0.0;
	l = 0;
	//printf("getimp\n");
	while (1) {
		l++;
		if (mx < val) {
			mx = val;
		}
		if (mx > wave_state.edge_threshold) {
			if (val < mx) {
			//printf("Implen: %d %f\n",l,val);
				break;
			}
		}
		val = GetNextDiff();
		if (val < -0xffff) {
			//printf("Implen: %d %f\n",l,val);
			Eof=1;
			return result;
		}
	}
	//printf("Implen: %d\n",l);
	result = l;
	return result;
}

float CalculateVolume() {
	int i;
	float val, sum;
	sum = 0;
	i = 0;

	val = GetNextSample();
	while (val > -0xffff) {
		sum += fabsf(val);
		i++;
		if (sum > 0x60000000) {
			break;
		}
		val = GetNextSample();
	}
	return sum / i;
}

uint8_t single_sector[512];

void ResetChecksum() {
	chksum = 0x57;
}
uint8_t CalcChecksum(int cntfrom, int cntto) {
	int i;
	uint16_t nck;
	uint16_t carry;
	nck = chksum;
	uint8_t result = chksum;
	for (i = cntfrom; i >= cntto; i--) {
		nck = nck + single_sector[i];
		carry = (nck & 0x100) / 0x100;
		nck = nck * 2 + carry;
		carry = (nck & 0x100) / 0x100;
		nck = nck + carry;
		nck = nck & 0xff;
	}
	chksum = nck;
	result = chksum;
	return result;
}
void DecodeBytes(int num) {
	int i, bit, byt, val;
	for (i = num - 1; i >= 0; i--) {
		byt = 0;
		bit = 0x80;
		while (bit > 0) {
			val = GetImpulseLength();
			if (val > 7) {
				return;
			}
			if (val > 4) {
				byt = byt | bit;
			}
			bit = bit / 2;
		}
		single_sector[i] = byt;
	}
}
int FindHeader() {
	int cnt, val;
	cnt = 8;
	int result = 0;
	int count=0;
	while (1) {
		val = GetImpulseLength();

		if (val == 0) {
			return result;
		}
		if (val >= 7 && val <= 10) {
			cnt--;
			if (cnt == 0) {
				val = GetImpulseLength();
				if (val <= 5) {
					result = 1;
					return result;
				}
				cnt = 8;
			}
		} else {
			cnt = 8;
		}
	}
}

void outSector(unsigned char * single_sector,int len)
{
	return ;
	for (int i=0; i<len; i++)
	{
		char outch[33];
		outch[32]=0;
		unsigned char c=single_sector[i];
		printf("%02x ",c);

		outch[i&0x1f]='.';
		if ((c&0x7f)>=32) outch[i&0x1f]=c;
		
		if ((i&0x1f)==0x1f) printf(" %s\n",outch);
	}
}
SStatus DecodeSector() {
	int i;
	int sectorlength;
	int sectornum;
	int sector_addr;
	char strn[201];

	for (i = 0; i < sizeof(single_sector); i++) {
		single_sector[i] = 0;
	}
	if (FindHeader()) {
		ResetChecksum();
		DecodeBytes(4);
		CalcChecksum(3, 0);
		sectorlength = single_sector[0] + 256 * single_sector[1];
		sectornum = single_sector[2] + 256 * single_sector[3];
		sprintf(strn, "Found sector: %d length: %d", sectornum, sectorlength);
		if (!((sectorlength == 128) || (sectorlength == 256) || (sectorlength == 512))) {
			return ssEnd;
		}
		if (sectorlength > xfd_sector_length) {
			xfd_sector_length = sectorlength;
		}
		if (sectornum >= MAX_SECT_NUM) {
			return ssEnd;
		}
		if (xfd_sector_status[sectornum] != ssGood) {
			xfd_sector_status[sectornum] = ssBad;
			DecodeBytes(sectorlength + 1);
			CalcChecksum(sectorlength, 1);
			if (chksum == single_sector[0]) {
				strcat(strn, " CRC OK");
				if (verbose)
					printf("%s\n", strn);
				if (sectornum < 4) {
					sector_addr = 128 * (sectornum - 1);
					outSector(&single_sector[1],128); 
				} else {
					sector_addr = xfd_sector_length * (sectornum - 4) + 3 * 128;
					outSector(&single_sector[1],xfd_sector_length); 
				}
				for (i = 1; i <= sectorlength; i++) {
					xfd_sector_data[sector_addr + i - 1] = single_sector[i];
				}

				xfd_sector_status[sectornum] = ssGood;
				if (MaxSector < sectornum) {
					MaxSector = sectornum;
				}
				return ssGood;
			}
		}
	} else {
		return ssEnd;
	}
	return ssEnd;
}

void process_file(char * fname) {
	uint32_t wav_bytelength;
	float* wav_buffer;
	int i, z;
	SStatus sector_status;
	int GoodSectors, BadSectors, MissingSectors;
	long int tmp_thr;
	TWaveState tmp_ws;
	float vol, vol2;
	int init_threshold;
	long int tbadsects;
	int tail;

	#define NUM_CHANNELS	1
	#define SAMPLE_RATE 44100

	TinyWav tw;
	tinywav_open_read(&tw,fname, TW_INLINE);
	if (tw.numChannels != NUM_CHANNELS || tw.h.SampleRate != SAMPLE_RATE) {
		fprintf(stderr,"Supplied test wav has wrong format - should be [%d]channels, fs=[%d]\n", NUM_CHANNELS, SAMPLE_RATE);
		exit(-1);
	}

	wave_state.resol = (tw.h.BitsPerSample & 0x1f) / 8;
	if (wave_state.resol==1)

	wave_state.levels = 256;
	if (wave_state.resol == 2) {
		wave_state.levels *= 256;
	}
	wave_state.next = tw.numChannels * sizeof(float);
	wave_state.endian = (tw.h.Format & 0x1000) / 0x1000;
	wave_state.ssigned = (tw.h.Format & 0x8000) / 0x8000;
	wave_state.bytelength = wav_bytelength;
	wave_state.framelength =tw.numFramesInHeader;
	wave_state.prev_val = 0;
	wave_state.curr_val = 0;
	if (verbose) {
		printf("length byt : %d\n", wav_bytelength);
		printf("channels   : %d\n", tw.numChannels);
		printf("frequency  : %d\n", tw.h.SampleRate);
		printf("resolution : %d\n", wave_state.resol * 8);
		printf("levels     : %d\n", wave_state.levels);
		printf("signed     : %s\n", wave_state.ssigned > 0 ? "yes" : "no");
		printf("big endian : %s\n", wave_state.endian > 0 ? "yes" : "no");
	}

	int totalNumSamples = tw.numFramesInHeader;
	assert((wav_buffer=malloc(totalNumSamples*tw.numChannels*sizeof(float)))>0 && "Could not alloc memory");

	if (verbose) {
		printf("Size of buffer: %d\n",totalNumSamples*tw.numChannels*wave_state.resol);
		printf("Samples: %d\n",totalNumSamples);
	}

	int samplesRead = tinywav_read_f(&tw, wav_buffer, totalNumSamples);
	if (samplesRead ==0)
	{
		if(verbose)
			fprintf(stderr,"Could not read from file!\n");
		exit(1);
	}


	/*
	for (int i=0; i<256; i++){
		printf("%f ",*((float*)(wav_buffer)+i));
		if ((i&0xf)==0xf) printf("\n");
	}
	*/

	wave_state.curr_ptr = wav_buffer;
	wave_state.start_ptr = wav_buffer;

	wave_state.edge_threshold = CalculateVolume()+t_corr;
	if (verbose)
		printf("Volume: %f\n",wave_state.edge_threshold);

	// reinit
	wave_state.curr_ptr = wave_state.start_ptr;
	/*
	vol2 = 0;
	if (tw.numChannels == 2) {
		wave_state.curr_ptr = wave_state.start_ptr + wave_state.next / 2;
		vol2 = CalculateVolume();
		if (vol2 > vol) {
			vol = vol2;
			wave_state.curr_ptr = wave_state.start_ptr + wave_state.next / 2;
			printf("Using louder channel 2\n");
		} else {
			wave_state.curr_ptr = wave_state.start_ptr;
			printf("Using louder channel 1\n");
		}
	}
	*/

	tail = 1;
	BadSectors = 0;
	do {
		GoodSectors = 0;
		tbadsects = BadSectors;
		BadSectors = 0;
		MissingSectors = 0;
		if (verbose)
			printf("Trying threshold...%f\n", wave_state.edge_threshold);
		do {
			sector_status = DecodeSector();
			if (Eof) break;
		} while (sector_status == ssEnd);


	} while (!Eof);

	for (i = 1; i <= MaxSector; i++) {
		if (xfd_sector_status[i] == ssGood) {
			GoodSectors++;
		}
		if (xfd_sector_status[i] == ssBad) {
			BadSectors++;
		}
		if (xfd_sector_status[i] == ssEmpty) {
			MissingSectors++;
		}
	}
	if (verbose) {
		printf("Good Sectors    : %d\n", GoodSectors);
		printf("Bad Sectors     : %d\n", BadSectors);
		printf("Missing Sectors : %d\n", MissingSectors);
		printf("Max Sector num  : %d\n", MaxSector);
	}

	if ((BadSectors > 0) || (MissingSectors > 0)) {
		if (verbose)
			printf("WARNING. The image is incomplete!\n");
		incomplete=1;

	}
	if (verbose)
		printf("Finished\n");
}

void ResetAll() {
	int i;
	for (i = 0; i < sizeof(xfd_sector_status) / sizeof(xfd_sector_status[0]); i++) {
		xfd_sector_status[i] = ssEmpty;
	}
	MaxSector = 0;
}

int SaveDiskImage(char* lfname, int header) {
	int subtract, res, headerlen, i;
	FILE* f;
	f = fopen(lfname, "wb");
	if (f == NULL) {
		if (verbose)
			printf("File save failed.\n");
		return 1;
	}
	headerlen = 0;
	subtract = 0;
	if (xfd_sector_length == 256) {
		subtract = 3 * 128;
	}
	if (header) {
		// create room for header.
		for (i = sizeof(xfd_sector_data) - 17; i >= 0; i--) {
			xfd_sector_data[i + 16] = xfd_sector_data[i];
		}
		headerlen = 16;
		for (i = 0; i < 16; i++) {
			xfd_sector_data[i] = 0;
		}
		xfd_sector_data[0] = 0x96;
		xfd_sector_data[1] = 0x02;
		xfd_sector_data[2] = ((MaxSector * xfd_sector_length - subtract) / 16) & 0xff;
		xfd_sector_data[3] = ((MaxSector * xfd_sector_length - subtract) / 16) / 0x100;
		xfd_sector_data[4] = xfd_sector_length & 0xff;
		xfd_sector_data[5] = xfd_sector_length / 0x100;
	}
	res = fwrite(xfd_sector_data, MaxSector * xfd_sector_length - subtract + headerlen, 1, f);
	fclose(f);
	if (1 == res) {
		if (verbose)
			printf("File %s saved.\n",lfname);
	} else {
		if (verbose)
			printf("File %s save failed\n",lfname);
		return 1;
	}
	return 0;
}

int SaveFile(char * fname,int hdr, char * ext) {
	int i;
	i=strrchr(fname,'.')-fname;
	if (i<1000){
		strncpy(savefname, fname, i+1);
		savefname[i+1] = '\0';
		strcat(savefname, ext);
		return SaveDiskImage(savefname, hdr);
	}
	else
	{
		if (verbose)
			printf("Filename error\n");
		return 1;
	}
	return 0;
}

void ShowDisk2AudBas() {
	const char* disk2snd = {
		"0 . DISK2SND COPIER\n"
		"1 . WITHOUT ANY CABLES.\n"
		"2 . BY JAKUB HUSAK, DATE:07.2012\n"
		"4 . JUST RETYPE THIS PROGGY,\n"
		"5 . PUT THE DISK INTO DRIVE 1\n"
		"6 . YOU WANT TO CONVERT TO XFD,\n"
		"7 . ENTER NUMBERS AND RECORD\n"
		"8 . OUTGOING NOISE ON PC, THEN\n"
		"9 . SAVE AS WAV AND FEED THE PC APP\n"
		"10 SUM=13218 : PLEN=145\n"
		"11 POKE 65,0:  DIM A$(4), B$(512)\n"
		"12 FOR I=1 TO PLEN*2 STEP 4: READ A$: B$(I,I+4)=A$:N.I\n"
		"13 FOR I=1 TO PLEN*2 STEP 2: VAL=(ASC(B$(I))-65)*16+ASC(B$(I+1))-65:POKE 1536+(I-1)/2,VAL:SUM=SUM-VAL:N.I\n"
		"14 IF SUM<>0 THEN ? \"DATA ERROR\": END\n"
		"15 ? \"START SEC?\": I. SSEC\n"
		"16 ? \"END SEC?\": I. ESEC\n"
		"17 ? \"SECT LEN [1]28/[2]56?\":I. SLEN\n"
		"18 FOR I=SSEC TO ESEC: ? \"READING SECTOR: \";I,\"ST: \";:? USR(1543,I,I,128+(I>3)*(SLEN=2)*128): N.I\n"
		"19 ? \"OPERATION COMPLETED.\" : END\n"
		"20 D. DBAB,FCEA,AIAE,BOKC,AGLN\n"
		"21 D. AAAG,JNAA,ADGI,JNAI,ADMK\n"
		"22 D. BAPD,CAFJ,OEIF,NFIE,NEBA\n"
		"23 D. ABGA,HICA,FIAG,KJFH,IFDB\n"
		"24 D. CAGI,AGKC,AJIN,AKNE,CAGC\n"
		"25 D. AGNA,PICA,GFAG,KAAE,KJAI\n"
		"26 D. IFHO,KJAD,IFHP,CAHG,AGOG\n"
		"27 D. HPKM,AIAD,CAHG,AGKF,DBKA\n"
		"28 D. AACA,HJAG,KAEA,KJCC,IMAO\n"
		"29 D. NEIN,AANE,FIGA,JAAD,INAK\n"
		"30 D. NEKF,DFCM,KJBP,EJAP,IFDF\n"
		"31 D. INAK,NEIN,ABNC,MKGA,IILB\n"
		"32 D. HOIF,DCBI,GFDB,CKGJ,AAIF\n"
		"33 D. DBKC,AHAG,DCCA,GAAG,BAPJ\n"
		"34 D. MAAA,NAOG,GA\n"
	};
	printf("%s", disk2snd);
}

void usage(char * n){
	printf("Usage: %s [params] <input_file.wav>\n",n);
	printf("\nAuthor: Jakub Husak, 11.2023\n\nParams:\n"
	"-s: show AtariBasic encoder source\n"
	"-v: be verbose\n"
	"-f: force write incomplete disk image\n"
	"-t <val>: threshold (0.0-1.0), nostly works 0.2-0.4, default auto\n"
	);
	exit(64);
}

int main(int argc, char ** argv) {
	char * filename=NULL;
	if (argc==1) usage(argv[0]);
	for (int i=1; i<argc; i++)
	{
		if (strncmp(argv[i],"-s",3)==0) {
			ShowDisk2AudBas();
			exit(0);
		}
		else if (strncmp(argv[i],"-f",3)==0) {
			force_save=1;
		}
		else if (strncmp(argv[i],"-v",3)==0) {
			verbose=1;
		}
		else if (strncmp(argv[i],"-t",3)==0) {
			if (i<argc-1) {
				t_corr=atof(argv[++i]);
			}
			else
				usage(argv[0]);
		}
		else
		{
			filename=argv[i];
		}
	}

	if (!filename) usage(argv[0]);

	process_file(filename);

	if (!incomplete || force_save) {
		if (SaveFile(filename,1,"atr")!=0) fprintf(stderr,"ATR file write error\n");
		if (SaveFile(filename,0,"xfd")!=0) fprintf(stderr,"XFD file write error\n");
	}
	else
	{
		if (verbose)
			printf("Output not written. If you want to write it anyway, please add -f flag to command line.\n");
		return(1);
	}
	return 0;
}



