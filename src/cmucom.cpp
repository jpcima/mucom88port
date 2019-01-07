
//
//		MUCOM88 access class
//		(PC-8801��VM�����MUCOM88�̋@�\��񋟂��܂�)
//			MUCOM88 by Yuzo Koshiro Copyright 1987-2019(C) 
//			Windows version by onion software/onitama 2018/11
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "cmucom.h"
#include "mucomvm.h"
#include "mucomerror.h"
#include "md5.h"

#include "plugin.h"

#include "bin_music2.h"

#include "bin_expand.h"
#include "bin_errmsg.h"
#include "bin_msub.h"
#include "bin_muc88.h"
#include "bin_ssgdat.h"
#include "bin_time.h"
#include "bin_voice.h"
#include "bin_smon.h"


#define PRINTF vm->Msgf

static int htoi_sub(char hstr)
{
	//	exchange hex to int

	char a1;
	a1 = tolower(hstr);
	if ((a1 >= '0') && (a1 <= '9')) return a1 - '0';
	if ((a1 >= 'a') && (a1 <= 'f')) return a1 - 'a' + 10;
	return 0;
}


static int htoi(char *str)
{
	//	16�i->10�i�ϊ�
	//
	char a1;
	int d;
	int conv;
	conv = 0;
	d = 0;
	while (1) {
		a1 = str[d++]; if (a1 == 0) break;
		conv = (conv << 4) + htoi_sub(a1);
	}
	return conv;
}

static int strpick_spc(char *target,char *dest,int strmax)
{
	//		str�̐擪����space�܂ł��������Ƃ��Ď��o��
	//
	unsigned char *p;
	unsigned char *dst;
	unsigned char a1;
	int len = 0;
	p = (unsigned char *)target;
	dst = (unsigned char *)dest;
	while (1) {
		if (len >= strmax) break;
		a1 = *p++; if (a1 == 0) return 0;
		if (a1 == 32) break;

		*dst++ = tolower(a1);
		len++;
		if (a1 >= 129) {					// �S�p�����`�F�b�N
			if ((a1 <= 159) || (a1 >= 224)) {
				p++; len++;
			}
		}
	}
	*dst++ = 0;
	return len;
}

/*------------------------------------------------------------*/
/*
		interface
*/
/*------------------------------------------------------------*/

CMucom::CMucom( void )
{
	flag = 0;
	vm = NULL;
	infobuf = NULL;
	user_uuid[0] = 0;
}


CMucom::~CMucom( void )
{
	Stop(1);
	Mucom88Plugin_Term();
	DeleteInfoBuffer();
	MusicBufferTerm();
	if (vm != NULL) delete vm;
}


void CMucom::Init(void *window, int option)
{
	//		MUCOM88�̏�����(���񂾂��Ăяo���Ă�������)
	//		window : 0   = �E�C���h�E�n���h��(HWND)
	//		               (NULL�̏ꍇ�̓A�N�e�B�u�E�C���h�E���I�������)
	//		option : 0   = 1:FM���~���[�g  2:SCCI���g�p
	//
	vm = new mucomvm;
	flag = 1;

	vm->SetOption(option);
	vm->InitSoundSystem();
	MusicBufferInit();

	Mucom88Plugin_Init(window,vm,this);
}


void CMucom::Reset(int option)
{
	//		MUCOM88�̃��Z�b�g(���x�ł��Ăׂ܂�)
	//		option : 0   = �����̃v���C���[������
	//		         1   = �O���t�@�C���ɂ�鏉����
	//		         2,3 = �R���p�C����������
	//
	int devres;
	vm->Reset();
	PRINTF("#OpenMucom88 Ver.%s Copyright 1987-2019(C) Yuzo Koshiro\r\n",VERSION);
	pcmfilename[0] = 0;

	devres = vm->DeviceCheck();
	if (devres) {
		PRINTF("#Device error(%d)\r\n", devres);
	}

	if (option & 2) {
		if (option & 1) {
			//	�R���p�C�����t�@�C������ǂ�
			vm->LoadMem("expand", 0xab00, 0);
			vm->LoadMem("errmsg", 0x8800, 0);
			vm->LoadMem("msub", 0x9000, 0);
			vm->LoadMem("muc88", 0x9600, 0);
			vm->LoadMem("ssgdat", 0x5e00, 0);
			vm->LoadMem("time", 0xe400, 0);
			vm->LoadMem("smon", 0xde00, 0);
			vm->LoadMem(MUCOM_DEFAULT_VOICEFILE, 0x6000, 0);
		}
		else {
			//	�����̃R���p�C����ǂ�
			vm->SendMem(bin_expand, 0xab00, expand_size);
			vm->SendMem(bin_errmsg, 0x8800, errmsg_size);
			vm->SendMem(bin_msub, 0x9000, msub_size);
			vm->SendMem(bin_muc88, 0x9600, muc88_size);
			vm->SendMem(bin_ssgdat, 0x5e00, ssgdat_size);
			vm->SendMem(bin_time, 0xe400, time_size);
			vm->SendMem(bin_voice_dat, 0x6000, voice_dat_size);
			vm->SendMem(bin_smon, 0xde00, smon_size);
		}

		int i;
		// music���̃_�~�[���[�`��
		for (i = 0; i < 0x40; i++) {
			vm->Poke(0xb000 + i, 0xc9);
		}
		// basic rom���̃_�~�[���[�`��
		for (i = 0; i < 0x4000; i++) {
			vm->Poke(0x1000 + i, 0xc9);
		}
		// ���[�N�ݒ�p���[�`��
		i = 0xb036;
		vm->Poke(i++, 0xdd);	// LD IX,$c9bf
		vm->Poke(i++, 0x21);
		vm->Poke(i++, 0x0c);
		vm->Poke(i++, 0xbf);
		vm->Poke(i++, 0xc9);

		return;
	}
	if (option & 1) {
		//	�v���C���[���t�@�C������ǂ�
		vm->LoadMem("music", 0xb000, 0);
	}
	else {
		//	�����̃v���C���[��ǂ�
		vm->SendMem(bin_music2, 0xb000, music2_size);
	}

	DeleteInfoBuffer();

	int i,adr;
	vm->InitChData(MUCOM_MAXCH,MUCOM_CHDATA_SIZE);
	for (i = 0; i < MUCOM_MAXCH; i++){
		adr = vm->CallAndHaltWithA(0xb00c, i);
		vm->SetChDataAddress( i,adr );
	}
}

void CMucom::SetUUID(const char *uuid)
{
	if (uuid == NULL) {
		user_uuid[0] = 0;
		return;
	}
	strncpy(user_uuid,uuid,64);
	user_uuid[64] = '\0';
}


char *CMucom::GetMessageBuffer(void)
{
	return vm->GetMessageBuffer();
}

int CMucom::Play(int num)
{
	//		MUCOM88���y�Đ�
	//		num : 0   = ���yNo. (0�`15)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	MUBHED *hed;
	char *data;
	char *pcmdata;
	const char *pcmname;
	int datasize;
	int pcmsize;

	if ((num < 0) || (num >= MUCOM_MUSICBUFFER_MAX)) return -1;

	Stop();

	LoadTagFromMusic(num);

	CMemBuf *buf = musbuf[num];
	if (buf == NULL) return -1;

	hed = (MUBHED *)buf->GetBuffer();
	data = MUBGetData(hed, datasize);
	if (data == NULL) return -1;

	if (hed->pcmdata) {
		int skippcm = 0;
		pcmname = GetInfoBufferByName("pcm");
		if (pcmname[0] != 0) {
			//	���ɓ�����PCM���ǂݍ��܂�Ă��邩?
			if (strcmp(pcmname, pcmfilename) == 0) skippcm = 1;
		}
		if (skippcm==0) {
			//	���ߍ���PCM��ǂݍ���
			pcmdata = MUBGetPCMData(hed, pcmsize);
			vm->LoadPcmFromMem(pcmdata, pcmsize);
		}
	}

	vm->SendMem((const unsigned char *)data, 0xc200, datasize);

	PRINTF("#Play[%d]\r\n", num);
	vm->CallAndHalt(0xb000);
	//int vec = vm->Peekw(0xf308);
	//PRINTF("#INT3 $%x.\r\n", vec);

	vm->StartINT3();
	vm->SetIntCount(0);

	int jcount = hed->jumpcount;
	vm->SkipPlay(jcount);

	playflag = true;

	return 0;
}


int CMucom::LoadTagFromMusic(int num)
{
	//		MUCOM88���y�f�[�^����^�O�ꗗ���擾����
	//		num : 0   = ���yNo. (0�`15)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	MUBHED *hed;
	int tagsize;

	if ((num < 0) || (num >= MUCOM_MUSICBUFFER_MAX)) return -1;

	CMemBuf *buf = musbuf[num];
	if (buf == NULL) return -1;

	hed = (MUBHED *)buf->GetBuffer();

	if (hed->tagdata) {
		DeleteInfoBuffer();
		infobuf = new CMemBuf();
		infobuf->PutStr(MUBGetTagData(hed, tagsize));
		infobuf->Put((int)0);
	}

	return 0;
}


int CMucom::Stop(int option)
{
	//		MUCOM88���y�Đ��̒�~
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	playflag = false;
	if (option & 1) {
		vm->StopINT3();
		vm->CallAndHalt(0xb003);
		vm->ResetFM();
	}
	else {
		vm->StopINT3();
		vm->CallAndHalt(0xb003);
	}
	return 0;
}


int CMucom::Fade(void)
{
	//		MUCOM88���y�t�F�[�h�A�E�g
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	if (playflag == false) return -1;
	vm->CallAndHalt(0xb006);
	return 0;
}


int CMucom::LoadFMVoice(const char *fname)
{
	//		FM���F�f�[�^�ǂݍ���
	//		fname = FM���F�f�[�^�t�@�C�� (�f�t�H���g��MUCOM_DEFAULT_VOICEFILE)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	if (vm->LoadMem(fname, 0x6000, 0) >= 0) return 0;
	PRINTF("#Voice file not found [%s].\r\n", fname);
	return -1;
}


int CMucom::LoadPCM(const char * fname)
{
	//		ADPCM�f�[�^�ǂݍ���
	//		fname = PCM�f�[�^�t�@�C�� (�f�t�H���g��MUCOM_DEFAULT_PCMFILE)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	if (strcmp(pcmfilename,fname)==0) return 0;			// ���ɓǂݍ���ł���ꍇ�̓X�L�b�v
	strncpy(pcmfilename, fname, MUCOM_FILE_MAXSTR-1);
	if (vm->LoadPcm(fname) == 0) return 0;
	PRINTF("#PCM file not found [%s].\r\n", fname);
	return -1;
}

int CMucom::LoadMusic(const char * fname, int num)
{
	//		���y�f�[�^�ǂݍ���
	//		fname = ���y�f�[�^�t�@�C��
	//		num : 0   = ���yNo. (0�`15)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	if ((num < 0) || (num >= MUCOM_MUSICBUFFER_MAX)) return -1;

	CMemBuf *buf = new CMemBuf();
	if (buf->PutFile((char *)fname) < 0) {
		PRINTF("#MUSIC file not found [%s].\r\n", fname);
		delete buf;
		return -1;
	}

	if (musbuf[num] != NULL) {
		delete musbuf[num];
	}
	musbuf[num] = buf;
	//if (vm->LoadMem(fname, 0xc200, 0) >= 0) return 0;
	return 0;
}

void CMucom::MusicBufferInit(void)
{
	for (int i = 0; i < MUCOM_MUSICBUFFER_MAX; i++) {
		musbuf[i] = NULL;
	}
}

void CMucom::MusicBufferTerm(void)
{
	for (int i = 0; i < MUCOM_MUSICBUFFER_MAX; i++) {
		if (musbuf[i] != NULL) {
			delete musbuf[i];
			musbuf[i] = NULL;
		}
	}
}

int CMucom::GetStatus(int option)
{
	//		MUCOM�X�e�[�^�X�ǂݏo��
	//		option : 0  ��~=0/���t��=1
	//		         1  ���t�J�n����̊��荞�݃J�E���g
	//		         2  �X�g���[���Đ��ɂ�����������(ms)
	//		         3  ���W���[�o�[�W�����R�[�h
	//		         4  �}�C�i�[�o�[�W�����R�[�h
	//		(�߂�l��32bit����)
	//
	int i;
	switch (option) {
	case MUCOM_STATUS_PLAYING:
		if (playflag) return 1;
		break;
	case MUCOM_STATUS_INTCOUNT:
		return vm->GetIntCount();
	case MUCOM_STATUS_PASSTICK:
		return vm->GetPassTick();
	case MUCOM_STATUS_MAJORVER:
		return MAJORVER;
	case MUCOM_STATUS_MINORVER:
		return MINORVER;

	case MUCOM_STATUS_COUNT:
		i = vm->GetIntCount();
		if (maxcount) {
			i = i % maxcount;
		}
		return i;
	case MUCOM_STATUS_MAXCOUNT:
		return maxcount;
	case MUCOM_STATUS_MUBSIZE:
		return mubsize;
	case MUCOM_STATUS_MUBRATE:
		return mubsize * 100 / MUCOM_MUBSIZE_MAX;
	case MUCOM_STATUS_BASICSIZE:
		return basicsize;
	case MUCOM_STATUS_BASICRATE:
		return basicsize * 100 / MUCOM_BASICSIZE_MAX;

	default:
		break;
	}
	return 0;
}


/*------------------------------------------------------------*/
/*
Compiler support
*/
/*------------------------------------------------------------*/

const char *CMucom::GetTextLine(const char *text)
{
	//	1�s���̃f�[�^���i�[
	//
	const unsigned char *p = (const unsigned char *)text;
	unsigned char a1;
	int mptr = 0;

	while (1) {
		a1 = *p++;
		if (a1 == 0) {
			p = NULL;  break;			// End of text
		}
		if (a1 == 9) {					// TAB->space
			a1 = 32;
		}
		if (a1 == 10) break;			// LF
		if (a1 == 13) {
			if (*p == 10) p++;
			break;						// CR/LF
		}

		if (a1 >= 129) {				// �S�p�����`�F�b�N
			if ((a1 <= 159) || (a1 >= 224)) {
				linebuf[mptr++] = a1;
				a1 = *p++;
			}
		}
		linebuf[mptr++] = a1;
	}
	linebuf[mptr++] = 0;
	return (char *)p;
}


const char *CMucom::GetInfoBuffer(void)
{
	if (infobuf == NULL) return "";
	return infobuf->GetBuffer();
}


const char *CMucom::GetInfoBufferByName(const char *name)
{
	//		infobuf���̎w��^�O���ڂ��擾
	//		name = �^�O��(�p������)
	//		(���ʂ�""�̏ꍇ�͊Y�����ڂȂ�)
	//
	int len;
	const char *src = GetInfoBuffer();
	while (1) {
		if (src == NULL) break;
		src = GetTextLine(src);

		len = strpick_spc((char *)linebuf+1, infoname, 63);
		if (len > 0) {
			//printf("[%s]\n", infoname);
			if (strcmp(name, infoname) == 0) {
				return (char *)(linebuf+1+len+1);
			}
		}
	}
	return "";
}


void CMucom::DeleteInfoBuffer(void)
{
	if (infobuf) {
		delete infobuf; infobuf = NULL;
	}
}


void CMucom::PrintInfoBuffer(void)
{
	//		infobuf�̓��e���o��
	//
	PRINTF("%s", GetInfoBuffer());
}


int CMucom::ProcessFile(const char *fname)
{
	//		MUCOM88 MML�\�[�X�t�@�C�����̃^�O������
	//		fname = MML�����̃e�L�X�g�f�[�^�t�@�C��(SJIS)
	//		(���ʂ́Ainfobuf�ɓ���܂�)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	int sz;
	char *mml;
	mml = vm->LoadAlloc(fname, &sz);
	if (mml == NULL) {
		PRINTF("#File not found [%s].\r\n", fname);
		return -1;
	}
	ProcessHeader( mml );
	free(mml);
	return 0;
}


int CMucom::ProcessHeader(char *text)
{
	//		MUCOM88 MML�\�[�X���̃^�O������
	//		text         = MML�����̃e�L�X�g�f�[�^(SJIS)(�I�[=0)
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	//		#mucom88 1.5
	//		#voice voice.dat
	//		#pcm mucompcm.bin
	//		#composer name
	//		#author name
	//		#title name
	//		#date xxxx/xx/xx
	//		#comment ----
	//		#url ----
	//
	const char *src = text;
	DeleteInfoBuffer();
	infobuf = new CMemBuf();
	while (1) {
		if (src == NULL) break;
		src = GetTextLine(src);
		if (linebuf[0] == '#') {
			//printf("[%s]\n", linebuf);
			infobuf->PutStr((char *)linebuf);
			infobuf->PutCR();
		}
	}
	infobuf->Put((char)0);
	return 0;
}


int CMucom::StoreBasicSource(const char *text, int line, int add)
{
	//	BASIC�\�[�X�̌`���Ń��X�g���쐬
	//
	const char *src = text;
	int ln = line;
	int mptr = 1;
	int linkptr;
	int i;
	unsigned char a1;

	while (1) {
		if (src == NULL) break;

		linkptr = mptr;
		mptr += 2;
		vm->Pokew(mptr,ln);
		mptr += 2;
		vm->Poke(mptr, 0x3a);
		vm->Poke(mptr+1, 0x8f);
		vm->Poke(mptr+2, 0xe9);
		mptr += 3;

		src = GetTextLine(src);
		i = 0;
		while (1) {
			a1 = linebuf[i++];
			vm->Poke(mptr++, a1);
			if (a1 == 0) break;
		}
		vm->Pokew(linkptr, mptr);		// ���̃|�C���^��ۑ�����
		ln += add;
	}

	vm->Pokew(mptr, 0);					// End pointer
	return mptr;
}


int CMucom::Compile(char *text, const char *filename, int option)
{
	//		MUCOM88�R���p�C��(Reset���K�v)
	//		text         = MML�����̃e�L�X�g�f�[�^(SJIS)(�I�[=0)
	//		filename     = �o�͂���鉹�y�f�[�^�t�@�C��
	//		option : 1   = #�^�O�ɂ��voice�ݒ�𖳎�
	//		         2   = PCM���ߍ��݂��X�L�b�v
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	int i, res;
	int start, length;
	char *adr_start;
	char *adr_length;
	int workadr;
	int pcmflag;

	maxch = MUCOM_MAXCH;				// ���[�N�̎擾��������ߌŒ�l

	AddExtraInfo(text);

	//		voice�^�O�̉��
	if ((option & 1) == 0) {
		char voicefile[MUCOM_FILE_MAXSTR + 1];
		strncpy(voicefile, GetInfoBufferByName("voice"), MUCOM_FILE_MAXSTR);
		if (voicefile[0]) {
			voicefile[MUCOM_FILE_MAXSTR] = '\0';
			LoadFMVoice(voicefile);
		}
	}

	basicsize = StoreBasicSource( text, 1, 1 );
	vm->CallAndHalt(0x9600);
	int vec = vm->Peekw(0x0eea8);
	PRINTF("#poll a $%x.\r\n", vec);

	int loopst = 0xf25a;
	vm->Pokew( loopst, 0x0101 );		// ���[�v���X�^�b�N������������(���[�v�O��'/'�ŃG���[���o������)

	res = vm->CallAndHalt2(vec, 'A');
	if (res){
		int line = vm->Peekw(0x0f32e);
		int msgid = vm->GetMessageId();
		if (msgid>0) {
			PRINTF("#error %d in line %d.\r\n-> %s (%s)\r\n", msgid, line, mucom_geterror_j(msgid), mucom_geterror(msgid));
		}
		else {
			PRINTF("#unknown error in line %d.\r\n", line);
		}
		return -1;
	}
	
	char stmp[128];
	vm->PeekToStr(stmp,0xf3c8,80);		// ��ʍŏ�i�̃��b�Z�[�W
	PRINTF("%s\r\n", stmp);

	workadr = 0xf320;
	fmvoice = vm->Peek(workadr + 50);
	pcmflag = 0;
	maxcount = 0;
	mubsize = 0;

	jumpcount = vm->Peekw(0x8c90);		// JCLOCK�̒l(J�R�}���h�̃^�O�ʒu)
	jumpline = vm->Peekw(0x8c92);		// JPLINE�̒l(J�R�}���h�̍s�ԍ�)

	PRINTF("Used FM voice:%d", fmvoice);

	if (jumpcount > 0) {
		PRINTF("  Jump to line:%d", jumpline);
	}

	PRINTF("\r\n");
	PRINTF("[ Total count ]\r\n");

	for (i = 0; i < maxch; i++){
		int tcnt,lcnt;
		tcnt = vm->Peekw(0x8c10 + i * 4);
		lcnt = vm->Peekw(0x8c12 + i * 4);
		if (lcnt) { lcnt = tcnt - (lcnt - 1); }
		if (tcnt > maxcount) maxcount = tcnt;
		tcount[i] = tcnt;
		lcount[i] = lcnt;
		PRINTF("%c:%d ", 'A' + i, tcount[i]);
	}
	PRINTF("\r\n");

	PRINTF("[ Loop count  ]\r\n");

	for (i = 0; i < maxch; i++){
		PRINTF("%c:%d ", 'A' + i, lcount[i]);
	}
	PRINTF("\r\n");

	adr_start = stmp + 31;
	adr_start[4] = 0;
	start = htoi(adr_start);

	adr_length = stmp + 41;
	adr_length[4] = 0;
	length = htoi(adr_length);

	mubsize = length;

	PRINTF("#Data Buffer $%04x-$%04x ($%04x)\r\n", start, start + length - 1,length);
	PRINTF("#MaxCount:%d Basic:%04x Data:%04x\r\n", maxcount, basicsize, mubsize);

	if (tcount[maxch-1]==0) pcmflag = 2;	// PCM ch���g���ĂȂ����PCM���ߍ��݂̓X�L�b�v

	return SaveMusic(filename,start,length,option| pcmflag);
}


int CMucom::CompileFile(const char *fname, const char *sname, int option)
{
	//		MUCOM88�R���p�C��(Reset���K�v)
	//		fname     = MML�����̃e�L�X�g�t�@�C��(SJIS)
	//		sname     = �o�͂���鉹�y�f�[�^�t�@�C��
	//		option : 1   = #�^�O�ɂ��voice�ݒ�𖳎�
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	int res;
	int sz;
	char *mml;
	PRINTF("#Compile[%s] to %s.\r\n", fname, sname);
	mml = vm->LoadAlloc(fname, &sz);
	if (mml == NULL) {
		PRINTF("#File not found [%s].\r\n", fname);
		return -1;
	}

	ProcessHeader(mml);

	res = Compile(mml, sname, option);
	free(mml);

	if (res) return res;

	return 0;
}


void CMucom::AddExtraInfo(char *mmlsource)
{
	//	infobuf�ɒǉ�������������
	//
	char mml_md5[128];
	if (infobuf == NULL) return;

	GetMD5(mml_md5, mmlsource, strlen(mmlsource));
	infobuf->PutStr("#mucver " VERSION );
	infobuf->PutCR();
	infobuf->PutStr("#mmlhash ");
	infobuf->PutStr(mml_md5);
	infobuf->PutCR();
	if (user_uuid[0] != 0) {
		infobuf->PutStr("#uuid ");
		infobuf->PutStr(user_uuid);
		infobuf->PutCR();
	}
}


int CMucom::SaveMusic(const char *fname,int start, int length, int option)
{
	//		���y�f�[�^�t�@�C�����o��(�R���p�C�����K�v)
	//		filename     = �o�͂���鉹�y�f�[�^�t�@�C��
	//		option : 1   = #�^�O�ɂ��voice�ݒ�𖳎�
	//		         2   = PCM���ߍ��݂��X�L�b�v
	//		(�߂�l��0�ȊO�̏ꍇ�̓G���[)
	//
	int res;
	MUBHED hed;
	char *header;
	char *footer;
	char *pcmdata;
	const char *pcmname;
	int hedsize;
	int footsize;
	int pcmsize;
	if (fname == NULL) return -1;

	header = (char *)&hed;
	hedsize = sizeof(MUBHED);
	memset(header,0,hedsize);
	header[0] = 'M';
	header[1] = 'U';
	header[2] = 'C';
	header[3] = '8';

	footer = NULL;
	pcmdata = NULL;
	footsize = 0;
	pcmsize = 0;
	if (infobuf) {
		infobuf->Put((int)0);
		footer = infobuf->GetBuffer();
		footsize = infobuf->GetSize();
	}

	hed.dataoffset = hedsize;
	hed.datasize = length;
	hed.tagdata = hedsize + length;
	hed.tagsize = footsize;
	hed.jumpcount = jumpcount;
	hed.jumpline = jumpline;

	if ((option & 2) == 0) {
		pcmname = GetInfoBufferByName("pcm");
		if (pcmname[0] != 0) {
			pcmdata = vm->LoadAlloc(pcmname, &pcmsize);
			if (pcmdata != NULL) {
				hed.pcmdata = hed.tagdata + footsize;
				hed.pcmsize = pcmsize;
			}
		}
	}

	//res = vm->SaveMem(filename, start, length);
	res = vm->SaveMemExpand(fname, start, length, header, hedsize, footer, footsize, pcmdata, pcmsize);
	if (res){
		PRINTF("#File write error [%s].\r\n", fname);
		return -2;
	}

	if (pcmdata != NULL) free(pcmdata);

	PRINTF("#Saved [%s].\r\n", fname);
	return 0;
}


char *CMucom::MUBGetData(MUBHED *hed, int &size)
{
	char *p;
	p = (char *)hed;
	if ((p[0] != 'M') || (p[1] != 'U') || (p[2] != 'C') || (p[3] != '8')) return NULL;
	p += hed->dataoffset;
	size = hed->datasize;
	return p;
}


char *CMucom::MUBGetTagData(MUBHED *hed, int &size)
{
	char *p;
	p = (char *)hed;
	p += hed->tagdata;
	if (hed->tagdata == 0) return NULL;
	size = hed->tagsize;
	if (size == 0) return NULL;
	return p;
}


char *CMucom::MUBGetPCMData(MUBHED *hed, int &size)
{
	char *p;
	p = (char *)hed;
	p += hed->pcmdata;
	if (hed->pcmdata == 0) { size = 0; return NULL; }
	size = hed->pcmsize;
	if (size == 0) return NULL;
	return p;
}


int CMucom::ConvertADPCM(const char *fname, const char *sname)
{
	int res;
	res = vm->ConvertWAVtoADPCMFile(fname, sname);
	if (res < 0) return -1;
	return 0;
}


void CMucom::GetMD5(char *res, char *buffer, int size)
{
	int di;
	md5_state_t state;
	md5_byte_t digest[16];
	char hex_output[16 * 2 + 1];

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)buffer, size);
	md5_finish(&state, digest);

	for (di = 0; di < 16; ++di) {
		sprintf(hex_output + di * 2, "%02x", digest[di]);
	}
	strcpy(res, hex_output);
}


void CMucom::SetVMOption(int option, int mode)
{
	switch (mode) {
	case 0:
		vm->SetOption(option);
		break;
	case 1:
		vm->SetOption( vm->GetOption() | option);
		break;
	case 2:
		vm->SetOption(vm->GetOption() & ~option);
		break;
	default:
		break;
	}
}


void CMucom::SetVolume(int fmvol, int ssgvol)
{
	vm->SetVolume(fmvol, ssgvol);
}


void CMucom::SetFastFW(int value)
{
	vm->SetFastFW(value);
}


int CMucom::GetChannelData(int ch, PCHDATA *result)
{
	int i;
	int size;
	int *ptr;
	unsigned char *src;
	unsigned char *srcp;
	bool ready = playflag;
	if ((ch < 0) || (ch >= MUCOM_MAXCH)) ready = false;
	if (!ready){
		memset(result, 0, sizeof(PCHDATA));
		return -1;
	}

	size = sizeof(PCHDATA)/sizeof(int);
	src = (unsigned char *)vm->GetChData(ch);
	srcp = src;
	ptr = (int *)result;

	if (src == NULL) return -1;

	for (i = 0; i < size; i++){
		*ptr++ = (int)*src++;
	}

	result->wadr = srcp[2] + (srcp[3] << 8);
	result->tadr = srcp[4] + (srcp[5] << 8);
	result->detune = srcp[9] + (srcp[10] << 8);
	result->lfo_diff = srcp[23] + (srcp[24] << 8);

	//	Check pan
	int pan = 0;
	unsigned char chwork;
	switch (ch) {
	case MUCOM_CH_PSG:
	case MUCOM_CH_PSG+1:
	case MUCOM_CH_PSG+2:
	case MUCOM_CH_RHYTHM:
		pan = 3;
		break;
	case MUCOM_CH_ADPCM:
		chwork = vm->GetChWork(15);
		pan = chwork & 3;
		break;
	default:
		if (ch >= MUCOM_CH_FM2) {
			chwork = vm->GetChWork(8 + 4 + (ch - MUCOM_CH_FM2));
		}
		else {
			chwork = vm->GetChWork(8 + ch);
		}
		pan = (int)(chwork & 0xc0);
		pan = pan >> 6;
		break;
	}
	result->pan = pan;

	return 0;
}

