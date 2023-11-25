#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

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
uint8_t xfd_sector_data[256 * MAX_SECT_NUM];
SStatus xfd_sector_status[6000];
int xfd_sector_length;
TWaveState wave_state;
char fname[1024];
char savefname[1024];
FILE* wav;
int Eof=0;

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

void AnalyzeLevels() {
	int i;
	float val;
	/*
	TWaveState twst = wave_state;
	for (i = 0; i <= 200; i++) {
		analyze_buffer[i] = 0;
	}
	val = GetNextDiff();
	while (val > -1.0 && val <=1.0) {
		analyze_buffer[(int)(val*100)+100]++;
		val = GetNextDiff();
	}

	i = 100;
	val = i;

	while (i > 104) {
		if (val > -1.0 && val <=1.0) {
			if (analyze_buffer[(int)(val*100)+100] <= analyze_buffer[i]) {
				val = i;
			}
		}
		i--;
	}
	printf("Max at:%f\n\n", val);
	wave_state = twst;
	*/
	wave_state.edge_threshold = val / 2;
	wave_state.edge_threshold = 0.20;
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

int CalculateVolume() {
	int i, val, sum;
	sum = 0;
	i = 0;

	val = GetNextSample();
	while (val > -0xffff) {
		sum += abs(val);
		i++;
		if (sum > 0x60000000) {
			break;
		}
		val = GetNextSample();
	}
	int result = sum / i;
	return result;
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

		if (count++>4000) printf("VAL: %d\n",val);
		if (val == 0) {
			return result;
		}
		if (val >= 7 && val <= 10) {
			cnt--;
			//if (cnt<7) printf("VAL: %d CNT: %d\n",val,cnt);
			if (cnt == 0) {
				val = GetImpulseLength();
				if (val <= 5) {
					result = 1;
					//printf("OK\n");
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
	int vol, vol2;
	float* tmp_ptr;
	int init_threshold;
	long int tbadsects;
	int tail;

	#define NUM_CHANNELS	1
	#define SAMPLE_RATE 44100

	TinyWav tw;
	tinywav_open_read(&tw,fname, TW_INLINE);
	if (tw.numChannels != NUM_CHANNELS || tw.h.SampleRate != SAMPLE_RATE) {
		printf("Supplied test wav has wrong format - should be [%d]channels, fs=[%d]\n", NUM_CHANNELS, SAMPLE_RATE);
		exit(-1);
	}

	wave_state.resol = (tw.h.Format & 0x1f) / 8;
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
	printf("length byt : %d\n", wav_bytelength);
	printf("channels   : %d\n", tw.numChannels);
	printf("frequency  : %d\n", tw.h.SampleRate);
	printf("SampFmt  : %d\n", tw.sampFmt);
	printf("resolution : %d\n", wave_state.resol * 8);
	printf("signed     : %s\n", wave_state.ssigned > 0 ? "yes" : "no");
	printf("big endian : %s\n", wave_state.endian > 0 ? "yes" : "no");

	int totalNumSamples = tw.numFramesInHeader;
	assert((wav_buffer=malloc(totalNumSamples*tw.numChannels*sizeof(float)))>0 && "Could not alloc memory");

	printf("Size of buffer: %d\n",totalNumSamples*tw.numChannels*wave_state.resol);
	printf("Samples: %d\n",totalNumSamples);

	int samplesRead = tinywav_read_f(&tw, wav_buffer, totalNumSamples);
	assert(samplesRead > 0 && " Could not read from file!");

	/*
	for (int i=0; i<256; i++){
		printf("%f ",*((float*)(wav_buffer)+i));
		if ((i&0xf)==0xf) printf("\n");
	}
	*/

	wave_state.curr_ptr = wav_buffer;
	wave_state.start_ptr = wav_buffer;

	wave_state.curr_ptr = wave_state.start_ptr;
	/*
	vol = CalculateVolume();
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
	printf("Volume: %d\n",vol);
	*/

	tmp_ptr = wave_state.curr_ptr;
	AnalyzeLevels();
	wave_state.curr_ptr = tmp_ptr;
	init_threshold = 0;
	tail = 1;
	BadSectors = 0;
	do {
		//tmp_ws = wave_state;
		GoodSectors = 0;
		tbadsects = BadSectors;
		BadSectors = 0;
		MissingSectors = 0;
		printf("Trying threshold...%f\n", wave_state.edge_threshold);
		do {
			sector_status = DecodeSector();
			if (Eof) break;
		} while (sector_status == ssEnd);

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
		printf("Good Sectors    : %d\n", GoodSectors);
		printf("Bad Sectors     : %d\n", BadSectors);
		printf("Missing Sectors : %d\n", MissingSectors);
		printf("Max Sector num  : %d\n", MaxSector);

		//wave_state = tmp_ws;
		if (init_threshold) {
			wave_state.edge_threshold = 0.016;
			init_threshold = 0;
		}
		/*
		if (tbadsects != BadSectors) {
			if (tail == 0) {
				wave_state.edge_threshold -= wave_state.edge_threshold / 4.0;
			}
			tail = 16;
		} else {
			if (tail > 0) {
				tail--;
			}
		}
		if (tail == 0) {
			wave_state.edge_threshold += wave_state.edge_threshold / 2;
		} else {
			wave_state.edge_threshold += wave_state.edge_threshold / 32 + 1;
		}
		*/
	} while (!Eof && ((BadSectors == 0) && (GoodSectors > 0) && (MissingSectors == 0)));

	if ((BadSectors > 0) || (MissingSectors > 0)) {
		printf("WARNING. The image is incomplete!\n");
	}
	printf("Finished\n");
}

void ResetAll() {
	int i;
	for (i = 0; i < sizeof(xfd_sector_status) / sizeof(xfd_sector_status[0]); i++) {
		xfd_sector_status[i] = ssEmpty;
	}
	MaxSector = 0;
}

void SaveDiskImage(char* lfname, int header) {
	int corr, res, headerlen, i;
	FILE* f;
	f = fopen(lfname, "wb");
	if (f == NULL) {
		printf("File save failed.\n");
		return;
	}
	headerlen = 0;
	corr = 0;
	if (xfd_sector_length == 256) {
		corr = 3 * 128;
	}
	if (header) {
		for (i = sizeof(xfd_sector_data) - 17; i >= 0; i--) {
			xfd_sector_data[i + 16] = xfd_sector_data[i];
		}
		headerlen = 16;
		for (i = 0; i < 16; i++) {
			xfd_sector_data[i] = 0;
		}
		xfd_sector_data[0] = 0x96;
		xfd_sector_data[1] = 0x02;
		xfd_sector_data[2] = ((MaxSector * xfd_sector_length - corr) / 16) & 0xff;
		xfd_sector_data[3] = ((MaxSector * xfd_sector_length - corr) / 16) / 0x100;
		xfd_sector_data[4] = xfd_sector_length & 0xff;
		xfd_sector_data[5] = xfd_sector_length / 0x100;
	}
	res = fwrite(xfd_sector_data, MaxSector * xfd_sector_length - corr + headerlen, 1, f);
	fclose(f);
	if (MaxSector * xfd_sector_length - corr + headerlen == res) {
		printf("File saved.\n");
	} else {
		printf("File save failed. (saved %d bytes)\n", res);
	}
	MaxSector = 0;
	ResetAll();
}

void SaveAtr(char * fname) {
	int i;
	i=strrchr(fname,'.')-fname;
	if (i<1000){
		strncpy(savefname, fname, i+1);
		savefname[i+1] = '\0';
		strcat(savefname, "atr");
		SaveDiskImage(savefname, 1);
		printf("saved: %s\n",savefname);
	}
}

void SaveXfd(char * fname) {
	int i;
	i=strrchr(fname,'.')-fname;
	if (i<1000){
		strncpy(savefname, fname, i+1);
		savefname[i+1] = '\0';
		strcat(savefname, "xfd");
		SaveDiskImage(savefname, 0);
		printf("saved: %s\n",savefname);
	}
}

void Btn_ShowDisk2AudBasClick() {
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

/* / not needed
void OutputLengths() {
	int cnt = 0;
	while (cnt < 10000) {
		cnt++;
		printf("%ld:%d\n", (wave_state.curr_ptr - wave_state.start_ptr) / 2, GetImpulseLength());
	}
}
*/

void usage(char * n){
	printf("Usage: %s [params] <input_file.wav>\n",n);
	printf("\nParams:\n"
	"-s: show AtariBasic encoder source\n"
	"-t: threshold (0.0-1.0), nostly works 0.2-0.4\n"
	);

}

int main(int argc, char ** argv) {
	process_file(argv[1]);
	SaveAtr(argv[1]);
	SaveXfd(argv[1]);
}



