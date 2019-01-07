
//
//		PC-8801 virtual machine
//		(PC-8801FA������CPU��OPNA�݂̂��G�~�����[�V�������܂�)
//			onion software/onitama 2018/11
//			Z80 emulation by Yasuo Kuwahara 2002-2018(C)
//			FM Sound Generator by cisc 1998, 2003(C)
//

#include <stdio.h>
#include <uv.h>
#include "mucomvm.h"
#include "adpcm.h"
#include "mucomerror.h"
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <unistd.h>

static void Sleep(int ms)
{
    usleep(ms * (useconds_t)1000);
}

#endif

#define RATE 55467				// Sampling Rate 55K
#define BUFSIZE 200				// Stream Buffer 200ms
#define baseclock 7987200		// Base Clock

#define USE_SCCI

/*------------------------------------------------------------*/
/*
interface
*/
/*------------------------------------------------------------*/

mucomvm::mucomvm(void)
{
	m_flag = 0;
	m_option = 0;
	m_fastfw = 4;
	m_fmvol = 0;
	m_ssgvol = -3;

	snddrv = NULL;
	opn = NULL;
	membuf = NULL;
	loop = uv_default_loop();
	timer = NULL;
	m_pChip = NULL;

	channel_max = 0;
	channel_size = 0;
	pchdata = NULL;

	soundblock = new float[2 * soundblocklen];
}

mucomvm::~mucomvm(void)
{
	if (playflag) {
		for (int i = 0; i < 300 && sending; i++) Sleep(10);
		playflag = false;
	}
	if (timer)
	{
		uv_timer_stop(timer);
		delete timer;
	}
	//	�X���b�h���~����
	delete snddrv;

	if (opn) {
		delete opn;
		opn = NULL;
	}
	if (membuf) delete membuf;

	if (pchdata) {
		free(pchdata);
	}

#ifdef USE_SCCI
	// ���A���`�b�v�p�N���X�J��
	if (m_pChip) {
		m_pChip->UnInitialize();
		delete m_pChip;
	}
#endif

	delete[] soundblock;
}


void mucomvm::SetOption(int option)
{
	m_flag = 0;
	m_option = option;
}


void mucomvm::SetVolume(int fmvol, int ssgvol)
{
	if (opn) {
		m_fmvol = fmvol;
		m_ssgvol = ssgvol;
		opn->SetVolumeFM(m_fmvol);
		opn->SetVolumePSG(m_ssgvol);
	}
}

void mucomvm::SetFastFW(int value)
{
	if (opn) {
		m_fastfw = value;
	}
}


void mucomvm::ResetFM(void)
{
	//	VM�̃��Z�b�g(FM���̂�)
	//
	if (playflag) {
		for (int i = 0; i < 300 && sending; i++) Sleep(10);
	}

	int i;
	for (i = 0; i < OPNACH_MAX; i++) {
		chmute[i] = 0; chstat[i] = 0;
	}
	memset(regmap,0,OPNAREG_MAX);

	if (opn) {
		int3mask = 0;
		int3flag = false;
		opn->Reset();
		opn->SetRate(baseclock, RATE, false);
		opn->SetReg(0x2d, 0);
		opn->SetVolumeFM(m_fmvol);
		opn->SetVolumePSG(m_ssgvol);
	}

#ifdef USE_SCCI
	if (m_pChip) {
		m_pChip->Reset();
		m_pChip->SetRegister(0x2d, 0);
	}
#endif

}

void mucomvm::InitSoundSystem(void)
{
	//	�T�E���h�̏�����(����݂̂�OK)
	//
	playflag = false;
	predelay = 0;
	sending = false;
	busyflag = false;
 
	//		SCCI�Ή�
	//
#ifdef USE_SCCI
	if (m_option & VM_OPTION_SCCI) {
		// ���A���`�b�v�p�N���X����
		m_pChip = new realchip();
		m_pChip->Initialize();
		if (m_pChip->IsRealChip() == false) {
			m_pChip = NULL;				// �������Ɏ��s������g�p���Ȃ�
		}
	}
#endif

	snddrv = new SoundDriver::DriverRT;
	snddrv->SetGenerator(this);
	snddrv->Init(RATE, 2, BUFSIZE);

	opn = new FM::OPNA;
	if (opn) {
		opn->Init(baseclock, 8000, 0);
#if 0
		// PSG TEST
		opn->SetReg(0x07, 8 + 16 + 32);
		opn->SetReg(0x08, 0x8);
		opn->SetReg(0x00, 0x0);
		opn->SetReg(0x01, 0x4);
#endif
		ResetFM();
	}

	//		�^�C�}�[������
	//
	time_master = 0;
	time_stream = 20;// buffer_length / num_blocks;
	time_intcount = 0;
	time_interrupt = 10;
	resetElapsedTime();

	if (timer) {
		uv_timer_stop(timer);
		delete timer;
	}

	timer = new uv_timer_t;
	timer->data = this;
	uv_timer_init(loop, timer);
	uv_timer_start(timer, TimeProc, 1, 1);

	//	�X�g���[���p�X���b�h
	playflag = true;
	//printf("#Stream update %dms.\n", time_stream);

	snddrv->SetReady();
}


void mucomvm::Reset(void)
{
	//	VM�̃��Z�b�g(FMReset���܂�)
	//
	uint8_t *p;

	Z80::Reset();
	ResetFM();
	SetIntVec(0xf3);

	p = &mem[0];
	memset(p, 0, 0x10000);		// CLEAR
	ClearBank();

	m_flag = VMFLAG_EXEC;
	sound_reg_select = 0;

#if 0
	*p++ = 0x3c;	    // LABEL1:INC A
	*p++ = 0xd3;
	*p++ = 0x01;		// OUT (0),A
	*p++ = 0x76;		// HALT
	*p++ = 0x18;
	*p++ = 0xfb;		// JR LABEL1
#endif

	if (membuf) delete membuf;
	membuf = new CMemBuf;
}


int mucomvm::DeviceCheck(void)
{
	//	�f�o�C�X�̐��퐫���`�F�b�N
	//
#ifdef USE_SCCI
	if (m_option & VM_OPTION_SCCI) {
		// ���A���`�b�v�����������삷�邩?
		if (m_pChip == NULL) return -1;
	}
#endif
	return 0;
}


int32_t mucomvm::load(uint16_t adr)
{
	return (int32_t)mem[adr];
}


void mucomvm::store(uint16_t adr, uint8_t data)
{
	mem[adr] = data;
}


int32_t mucomvm::input(uint16_t adr)
{
	//printf("input : %x\n", adr);
	int port = adr & 0xff;
	switch (port)
	{
	case 0x32:
		return int3mask;
	case 0x45:
		return FMInData();
	case 0x47:
		return FMInData2();
	default:
		break;
	}
	return 0;
}


void mucomvm::output(uint16_t adr, uint8_t data)
{
	//printf("output: %x %x\n", adr, data);
	int port = adr & 0xff;
	switch (port)
	{
	case 0x32:
		int3mask = (int)data;
		break;
	case 0x44:
		sound_reg_select = (int)data;
		break;
	case 0x45:
		FMOutData((int)data);
		break;
	case 0x46:
		sound_reg_select2 = (int)data;
		break;
	case 0x47:
		FMOutData2((int)data);
		break;
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f:
		ChangeBank( port - 0x5c );
		break;
	default:
		break;
	}
}

void mucomvm::Halt(void)
{
	//if (m_flag == VMFLAG_EXEC) printf("Halt.\n");
	m_flag = VMFLAG_HALT;
}


/*------------------------------------------------------------*/
/*
status
*/
/*------------------------------------------------------------*/

void mucomvm::ClearBank(void)
{
	int i;
	uint8_t *p;

	for (i = 0; i < VMBANK_MAX; i++) {
		p = &vram[i][0];
		memset(p, 0, VMBANK_SIZE);		// CLEAR
	}
	bankmode = VMBANK_MAX-1;
}


void mucomvm::ChangeBank(int bank)
{
	uint8_t *p;
	if (bankmode == bank) return;
	//	���݂̃��������e��ޔ�
	p = &vram[bankmode][0];
	memcpy(p, mem+0xc000, VMBANK_SIZE);
	//	�����炵���o���N�f�[�^���R�s�[
	p = &vram[bank][0];
	memcpy(mem + 0xc000,p, VMBANK_SIZE);
	bankmode = bank;
}


void mucomvm::BackupMem(uint8_t *mem_bak)
{
	memcpy( mem_bak, mem, 0x10000 );
}

void mucomvm::RestoreMem(uint8_t *mem_bak)
{
	memcpy(mem, mem_bak, 0x10000);
}

int mucomvm::Peek(uint16_t adr)
{
	return (int)mem[adr];
}


int mucomvm::Peekw(uint16_t adr)
{
	int res;
	res = ((int)mem[adr+1])<<8;
	res += (int)mem[adr];
	return res;
}


void mucomvm::Poke(uint16_t adr, uint8_t data)
{
	mem[adr] = data;
}


void mucomvm::Pokew(uint16_t adr, uint16_t data)
{
	mem[adr] = data & 0xff;
	mem[adr+1] = (data>>8) & 0xff;
}


void mucomvm::PeekToStr(char *out, uint16_t adr, uint16_t length)
{
	int i;
	for (i = 0; i < 80; i++) {
		out[i] = Peek(adr + i);
	}
	out[i] = 0;
}


int mucomvm::ExecUntilHalt(int times)
{
	int cnt=0;
	int id = 0;
	msgid = 0;
	while (1) {
#ifdef DEBUGZ80_TRACE
		//membuf->Put( (int)pc );
		Msgf("%06x PC=%04x HL=%04x A=%02x\r\n", cnt, pc, GetHL(), GetA());
#endif
		//printf("PC=%04x HL=%04x (%d):\n", pc, GetHL(), Peek(0xa0f5));
		if (pc < 0x8000) {
			if (pc == 0x5550) {
				int i;
				uint8_t a1;
				uint16_t hl = GetHL();
				char stmp[256];
				i = 0;
				while (1) {
					if (i >= 250) break;
					a1 = mem[hl++];
					if (a1 == 0) break;
					stmp[i++] = (char)a1;
				}
				stmp[i++] = 0;
				Poke(pc, 0xc9);
				id = mucom_geterror(stmp);
				if (id > 0) {
					msgid = id;
					//Msgf("#%s\r\n", mucom_geterror(msgid),msgid);
				}
				else {
					Msgf("#Unknown message [%s].\r\n", stmp);
				}
			}
			else if (pc == 0x3b3) {
				Msgf("#Error trap at $%04x.\r\n", pc);
				//DumpBin(0, 0x100);
				return -1;
			}
			else {
				Msgf("#Unknown ROM called $%04x.\r\n", pc);
				//return -2;
			}
		}

		if (pc==0xaf80) {				// expand����CULC: ��u��������
			int amul = GetA();
			int val = Peekw(0xAF93);
			int frq = Peekw(0xAFFA);
			int ans,count;
			float facc;
			float frqbef = (float)frq;
			if (val == 0x0A1BB) {
				facc = 0.943874f;
			}
			else {
				facc = 1.059463f;
			}
			for (count = 0; count < amul; count++) {
				frqbef = frqbef * facc;
			}
			ans = int(frqbef);
			SetHL(ans);
			//Msgf("#CULC A=%d : %d * %f =%d.\r\n", amul, frq, facc, ans);

			pc = 0xafb3;				// ret�̈ʒu�܂Ŕ�΂�
		}
#if 0
		if (pc==0x979c) {
			int ch = Peek(0x0f330);
			int line = Peekw(0x0f32b);
			int link = Peekw(0x0f354);
			printf("#CMPST $%04x LINKPT%04x CH%d LINE%04x.\r\n", pc, link, ch, line);
		}
		if ((pc >= 0xAD86) && (pc < 0xb000)) {
				printf("#VOICECONV1 %04x.\r\n", pc);
		}
#endif

		Execute(times);
		if (m_flag == VMFLAG_HALT) break;
		cnt++;
#if 0
		if ( cnt>=0x100000) {
			Msgf( "#Force halted.\n" );
			break;
		}
#endif
	}
#ifdef DEBUGZ80_TRACE
	membuf->SaveFile("trace.txt");
#endif
	//Msgf( "#CPU halted.\n" );
	return 0;
}


void mucomvm::DumpBin(uint16_t adr, uint16_t length)
{
	int i;
	uint16_t p = adr;
	uint16_t ep = adr+length;
	while (1) {
		if (p >= ep) break;
		Msgf("#$%04x", p);
		for (i = 0; i < 16; i++) {
			Msgf(" %02x", Peek(p++));
		}
		Msgf("\r\n");
	}
}


void mucomvm::Msgf(const char *format, ...)
{
	char textbf[4096];
	va_list args;
	va_start(args, format);
	vsprintf(textbf, format, args);
	va_end(args);
	membuf->PutStr(textbf);
}


void mucomvm::SetPC(uint16_t adr)
{
	pc = adr;
	m_flag = VMFLAG_EXEC;
}


void mucomvm::CallAndHalt(uint16_t adr)
{
	uint16_t tempadr = 0xf000;
	uint8_t *p = mem + tempadr;
	*p++ = 0xcd;				// Call
	*p++ = (adr & 0xff);
	*p++ = ((adr>>8) & 0xff);
	*p++ = 0x76;				// Halt
	SetPC(tempadr);
	ExecUntilHalt();
}


int mucomvm::CallAndHalt2(uint16_t adr,uint8_t code)
{
	uint16_t tempadr = 0xf000;
	uint8_t *p = mem + tempadr;
	*p++ = 0x21;				// ld hl
	*p++ = 0x10;
	*p++ = 0xf0;
	*p++ = 0xcd;				// Call
	*p++ = (adr & 0xff);
	*p++ = ((adr >> 8) & 0xff);
	*p++ = 0x76;				// Halt
	p = mem + tempadr + 16;
	*p++ = code;
	*p++ = 0;
	*p++ = 0;

	SetPC(tempadr);
	return ExecUntilHalt(1);
}


int mucomvm::CallAndHaltWithA(uint16_t adr, uint8_t areg)
{
	uint16_t tempadr = 0xf000;
	uint8_t *p = mem + tempadr;
	*p++ = 0x3e;				// ld a
	*p++ = areg;
	*p++ = 0xcd;				// Call
	*p++ = (adr & 0xff);
	*p++ = ((adr >> 8) & 0xff);
	*p++ = 0x76;				// Halt
	SetPC(tempadr);
	ExecUntilHalt();
	return (int)GetIX();
}


int mucomvm::LoadMem(const char *fname, int adr, int size)
{
	//	VM�������Ƀt�@�C�������[�h
	//
	FILE *fp;
	int flen,sz;
	fp = fopen(fname, "rb");
	if (fp == NULL) return -1;
	sz = size; if (sz == 0) sz = 0x10000;
	flen = (int)fread(mem+adr, 1, sz, fp);
	fclose(fp);
	Msgf("#load:%04x-%04x : %s (%d)\r\n", adr, adr + flen, fname, flen);
	return flen;
}


char *mucomvm::LoadAlloc(const char *fname, int *sizeout)
{
	//	�������Ƀt�@�C�������[�h
	//
	FILE *fp;
	char *buf;
	int sz;

	*sizeout = 0;
	fp = fopen(fname, "rb");
	if (fp == NULL) return NULL;
	fseek(fp, 0, SEEK_END);
	sz = (int)ftell(fp);			// normal file size
	if (sz <= 0) return NULL;
	buf = (char *)malloc(sz+16);
	if (buf) {
		fseek(fp, 0, SEEK_SET);
		fread(buf, 1, sz, fp);
		fclose(fp);
		buf[sz] = 0;
		Msgf("#load:%s (%d)\r\n", fname, sz);
		*sizeout = sz;
		return buf;
	}
	fclose(fp);
	return NULL;
}


int mucomvm::LoadPcmFromMem(const char *buf, int sz, int maxpcm)
{
	//	PCM�f�[�^��OPNA��RAM�Ƀ��[�h(����������)
	//
	char *pcmdat;
	char *pcmmem;
	int infosize;
	int i;
	int pcmtable;
	int inftable;
	int adr, whl, eadr;
	char pcmname[17];

	infosize = 0x400;
	inftable = 0xd000;
	SendMem((const unsigned char *)buf, inftable, infosize);
	pcmtable = 0xe300;
	for (i = 0; i < maxpcm; i++) {
		adr = Peekw(inftable + 28);
		whl = Peekw(inftable + 30);
		eadr = adr + (whl >> 2);
		if (buf[i * 32] != 0) {
			Pokew(pcmtable, adr);
			Pokew(pcmtable + 2, eadr);
			Pokew(pcmtable + 4, 0);
			Pokew(pcmtable + 6, Peekw(inftable + 26));
			memcpy(pcmname, buf + i * 32, 16);
			pcmname[16] = 0;
			Msgf("#PCM%d $%04x $%04x %s\r\n", i + 1, adr, eadr, pcmname);
		}
		pcmtable += 8;
		inftable += 32;
	}
	pcmdat = (char *)buf + infosize;
	pcmmem = (char *)opn->GetADPCMBuffer();
	memcpy(pcmmem, pcmdat, sz - infosize);

#ifdef USE_SCCI
	// ���A���`�b�v�Ή�
	if (m_pChip) {
		if (m_pChip->IsRealChip()) {
			m_pChip->SendAdpcmData(pcmdat, sz - infosize);
		}
	}
#endif

	return 0;
}


int mucomvm::LoadPcm(const char *fname,int maxpcm)
{
	//	PCM�f�[�^��OPNA��RAM�Ƀ��[�h
	//	(�ʓr�AMUCOM88��PCM�f�[�^���p�b�L���O�����f�[�^���K�v)
	//
	int sz;
	char *buf;
	buf = LoadAlloc( fname, &sz );
	if (buf) {
		LoadPcmFromMem( buf,sz,maxpcm );
		free(buf);
	}
	return 0;
}


int mucomvm::SendMem(const unsigned char *src, int adr, int size)
{
	//	VM�������Ƀf�[�^��]��
	//
	memcpy(mem + adr, src, size);
	return 0;
}


int mucomvm::SaveMem(const char *fname, int adr, int size)
{
	//	VM�������̓��e���t�@�C���ɃZ�[�u
	//
	FILE *fp;
	int flen;
	fp = fopen(fname, "wb");
	if (fp == NULL) return -1;
	flen = (int)fwrite(mem + adr, 1, size, fp);
	fclose(fp);
	return 0;
}


int mucomvm::SaveMemExpand(const char *fname, int adr, int size, char *header, int hedsize, char *footer, int footsize, char *pcm, int pcmsize)
{
	//	VM�������̓��e���t�@�C���ɃZ�[�u(�w�b�_�ƃt�b�^�t��)
	//
	FILE *fp;
	fp = fopen(fname, "wb");
	if (fp == NULL) return -1;
	if (header) fwrite(header, 1, hedsize, fp);
	fwrite(mem + adr, 1, size, fp);
	if (footer) fwrite(footer, 1, footsize, fp);
	if (pcm) fwrite(pcm, 1, pcmsize, fp);
	fclose(fp);
	return 0;
}


void mucomvm::checkThreadBusy(void)
{
	//		�X���b�h��VM���g�p���Ă��邩�`�F�b�N
	//
	for (int i = 0; i < 300 && sending; i++) Sleep(10);
	int3flag = false;
	for (int i = 0; i < 300 && busyflag; i++) Sleep(10);
}


void mucomvm::StartINT3(void)
{
	//		INT3���荞�݂��J�n
	//
	if (int3flag) {
		checkThreadBusy();
	}
	getElapsedTime();
	SetIntCount(0);
	predelay = 4;
	int3flag = true;
}


void mucomvm::StopINT3(void)
{
	//		INT3���荞�݂��~
	//
	checkThreadBusy();
	SetIntCount(0);
	getElapsedTime();
	predelay = 4;
}

void mucomvm::SkipPlay(int count)
{
	//		�Đ��̃X�L�b�v
	//
	if (count <= 0) return;

	int jcount = count;
	int vec = Peekw(0xf308);		// INT3 vector���擾

	SetChMuteAll(true);
	while (1) {
		if (jcount <= 0) break;
		CallAndHalt(vec);
		jcount--;
		time_intcount++;
#ifdef USE_SCCI
		if (m_pChip) {
			if ((jcount & 3) == 0) Sleep(1);	// ������Ƒ҂��܂�
		}
#endif
	}
	SetChMuteAll(false);
}


int mucomvm::getElapsedTime(void)
{
	int64_t cur_ft = uv_hrtime();
	int64_t pass_ft = cur_ft - last_ft;
	last_ft = cur_ft;
	double ms = pass_ft * 1e-6;
	pass_tick = (int)ms;
	return (int)(ms * 1024.0);
}


void mucomvm::resetElapsedTime(void)
{
	pass_tick = 0;
	last_ft = uv_hrtime();
}


void mucomvm::UpdateTime(void)
{
	//		1ms���Ƃ̊��荞��
	//
	int base;

	base = getElapsedTime();
	if (playflag == false) {
		return;
	}

	if (pass_tick <= 0) return;

	bool int3_mode = int3flag;
	time_master += pass_tick;//timer_period;

	if (int3mask & 128) int3_mode = false;		// ���荞�݃}�X�N

	if (opn->Count(base)) {
		if (int3_mode) {
			if (predelay == 0) {
				int times = 1;
				if (m_option & VM_OPTION_FASTFW) times = m_fastfw;
				busyflag = true;				// Z80VM��2�d�N����h�~����
				while (1) {
					if (times <= 0) break;
					int vec = Peekw(0xf308);	// INT3 vector���Ăяo��
					CallAndHalt(vec);
					times--;
					time_intcount++;
				}
				busyflag = false;
				ProcessChData();
			}
			else {
				predelay--;
			}
		}
	}

	//if (pass_tick>1) printf("INT%d (%d) %d\n", time_scount, pass_tick, time_master);

}


void mucomvm::StartGenerateSound()
{
	sending = true;
}

void mucomvm::EndGenerateSound()
{
	sending = false;
}

uint mucomvm::GenerateSoundBlock(float **data)
{
	// jpcima: what to do about INT3 flag?

	float *buf = soundblock;

	FM::Sample opnsamples[2 * soundblocklen];
	memset(opnsamples, 0, sizeof(opnsamples));
	if ((m_option & VM_OPTION_FMMUTE) == 0)
		opn->Mix(opnsamples, soundblocklen);
	else
		memset(opnsamples, 0, sizeof(opnsamples));

	for (uint i = 0; i < 2 * soundblocklen; ++i)
		buf[i] = opnsamples[i] * (1.0f / 32768);

	*data = buf;
	return soundblocklen;
}

//  TimeProc
//
void CALLBACK mucomvm::TimeProc(uv_timer_t *tm)
{
	mucomvm* inst = reinterpret_cast<mucomvm*>(tm->data);
	if (inst){
		//SetEvent(inst->hevent);
		inst->UpdateTime();
	}
}

/*------------------------------------------------------------*/
/*
FM synth
*/
/*------------------------------------------------------------*/

//	�f�[�^�o��
void mucomvm::FMOutData(int data)
{
	//printf("FMReg: %04x = %02x\n", sound_reg_select, data);
	switch (sound_reg_select) {
	case 0x28:
	{
		//		FM KeyOn
		int ch = data & 7;
		int slot = data & 0xf0;
		if (chmute[ch]) return;
		if (slot) chstat[ch] = 1; else chstat[ch] = 0;
		break;
	}
	case 0x10:
	{
		//		Rhythm KeyOn
		if (chmute[OPNACH_RHYTHM]) return;
		if (data & 0x80) chstat[OPNACH_ADPCM] = 1; else chstat[OPNACH_ADPCM] = 0;
		break;
	}
	default:
		break;
	}
	opn->SetReg(sound_reg_select, data);

#ifdef USE_SCCI
	if (m_pChip) {
		// ���A���`�b�v�o��
		m_pChip->SetRegister(sound_reg_select, data);
	}
#endif

}

//	�f�[�^�o��(OPNA��)
void mucomvm::FMOutData2(int data)
{
	//printf("FMReg2: %04x = %02x\n", sound_reg_select, data);

	if (sound_reg_select2 == 0x00) {
		if (chmute[OPNACH_ADPCM]) return;
		if (data & 0x80) chstat[OPNACH_ADPCM] = 1; else chstat[OPNACH_ADPCM] = 0;
	}

	opn->SetReg(sound_reg_select2 | 0x100, data);

#ifdef USE_SCCI
	if (m_pChip) {
		// ���A���`�b�v�o��
		m_pChip->SetRegister(sound_reg_select2 | 0x100, data);
	}
#endif
}

//	�f�[�^����
int mucomvm::FMInData(void)
{
	return (int)opn->GetReg(sound_reg_select);
}


//	�f�[�^����(OPNA��)
int mucomvm::FMInData2(void)
{
	return (int)opn->GetReg(sound_reg_select2|0x100);
}


void mucomvm::SetChMute(int ch, bool sw)
{
	chmute[ch] = (uint8_t)sw;
}


bool mucomvm::GetChMute(int ch)
{
	return (chmute[ch] != 0);
}


int mucomvm::GetChStatus(int ch)
{
	return (int)chstat[ch];
}


void mucomvm::SetChMuteAll(bool sw)
{
	int i;
	for (i = 0; i < OPNACH_MAX; i++) {
		SetChMute(i, sw);
	}
}


/*------------------------------------------------------------*/
/*
ADPCM
*/
/*------------------------------------------------------------*/

int mucomvm::ConvertWAVtoADPCMFile(const char *fname, const char *sname)
{
	Adpcm adpcm;
	DWORD dAdpcmSize;
	BYTE *dstbuffer;
	int sz, res;

	char *buf;
	buf = LoadAlloc(fname, &sz);
	if (buf == NULL) return -1;


	dstbuffer = adpcm.waveToAdpcm(buf, sz, dAdpcmSize, 16000 );
	free(buf);

	if (dstbuffer == NULL) return -1;
	res = (int)dAdpcmSize;

	FILE *fp;
	fp = fopen(sname, "wb");
	if (fp != NULL) {
		fwrite(dstbuffer, 1, res, fp);
		fclose(fp);
	}
	else {
		res = -1;
	}
	delete[] dstbuffer;
	return res;
}


/*------------------------------------------------------------*/
/*
Channel Data
*/
/*------------------------------------------------------------*/

void mucomvm::InitChData(int chmax, int chsize)
{
	if (pchdata) {
		free(pchdata);
	}
	channel_max = chmax;
	channel_size = chsize;
	pchdata = (uint8_t *)malloc(channel_max*channel_size);
	memset(pchdata, 0, channel_max*channel_size);
}


void mucomvm::SetChDataAddress(int ch, int adr)
{
	if ((ch < 0) || (ch >= channel_max)) return;
	pchadr[ch] = (uint16_t)adr;
}


uint8_t *mucomvm::GetChData(int ch)
{
	uint8_t *p = pchdata;
	if ((ch < 0) || (ch >= channel_max)) return NULL;
	if (pchdata) {
		return p + (ch*channel_size);
	}
	return NULL;
}


uint8_t mucomvm::GetChWork(int index)
{
	if ((index < 0) || (index >= 16)) return 0;
	return pchwork[index];
}


void mucomvm::ProcessChData(void)
{
	int i;
	uint8_t *src;
	uint8_t *dst;
	uint8_t code;
	uint8_t keyon;
	if (pchdata==NULL) return;
	if (channel_max == 0) return;
	for (i = 0; i < channel_max; i++){
		src = mem + pchadr[i];
		dst = pchdata + (channel_size*i);
		if (src[0] > dst[0]) {						// lebgth���X�V���ꂽ
			keyon = 0;
			code = src[32];
			if (i == 10) {
				keyon = src[1];						// PCMch�͉��FNo.������
			}
			else if ( i == 6) {
				keyon = mem[ pchadr[10] + 38 ];		// rhythm�̃��[�N�l������
			}
			else {
				if (code != dst[32]) {
					keyon = ((code >> 4) * 12) + (code & 15);	// ���K�������L�[No.
				}

			}
			src[37] = keyon;						// keyon����t������
		}
		else {
			if (src[0] < src[18]) {				// quantize�؂�
				src[37] = 0;						// keyon����t������
			}
		}
		memcpy(dst, src , channel_size);
	}
	memcpy( pchwork, mem + pchadr[0] - 16, 16 );	// CHDATA�̑O�Ƀ��[�N������
}


