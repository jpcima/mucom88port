
//
//	cmucomvm.cpp structures
//
#ifndef __mucomvm_h
#define __mucomvm_h

#include <stdio.h>
#include <atomic>
#include "Z80/Z80.h"
#include "fmgen/opna.h"
#include "soundrt.h"
#include "membuf.h"
#include "realchip.h"
#include "types.h"

typedef struct uv_loop_s uv_loop_t;
typedef struct uv_timer_s uv_timer_t;

//#define DEBUGZ80_TRACE

enum {
	VMFLAG_NONE, VMFLAG_EXEC, VMFLAG_HALT
};

#define VM_OPTION_FMMUTE 1
#define VM_OPTION_SCCI 2
#define VM_OPTION_FASTFW 4

#define VMBANK_MAX 4
#define VMBANK_SIZE 0x4000

#define OPNACH_MAX 16
#define OPNACH_FM 0
#define OPNACH_PSG 6
#define OPNACH_RHYTHM 9
#define OPNACH_ADPCM 10
#define OPNAREG_MAX 0x200

class mucomvm final : public Z80,
					  public SoundDriver::SoundBlockGenerator {
public:
	mucomvm();
	~mucomvm();

	//		Z80�R���g���[��
	void InitSoundSystem(void);
	void SetOption(int option);
	int GetOption(void) { return m_option; }
	void SetPC(uint16_t adr);
	void CallAndHalt(uint16_t adr);
	int CallAndHalt2(uint16_t adr, uint8_t code);
	int CallAndHaltWithA(uint16_t adr, uint8_t areg);

	//		���z�}�V���R���g���[��
	void Reset(void);
	void ResetFM(void);
	int LoadPcm(const char *fname, int maxpcm = 32);
	int LoadPcmFromMem(const char *buf, int sz, int maxpcm = 32);
	int LoadMem(const char *fname, int adr, int size);
	int SendMem(const unsigned char *src, int adr, int size);
	int SaveMem(const char *fname,int adr, int size);
	int SaveMemExpand(const char *fname, int adr, int size, char *header, int hedsize, char *footer, int footsize, char *pcm, int pcmsize);
	char *LoadAlloc(const char *fname, int *sizeout);
	void SetVolume(int fmvol, int ssgvol);
	void SetFastFW(int value);
	void SkipPlay(int count);

	int ExecUntilHalt(int times = 0x10000);

	//		���z�}�V���X�e�[�^�X
	int GetFlag(void) { return m_flag; }
	int Peek(uint16_t adr);
	int Peekw(uint16_t adr);
	void Poke(uint16_t adr, uint8_t data);
	void Pokew(uint16_t adr, uint16_t data);
	void PeekToStr(char *out, uint16_t adr, uint16_t length);
	void BackupMem(uint8_t *mem_bak);
	void RestoreMem(uint8_t *mem_bak);

	void StartINT3(void);
	void StopINT3(void);
	void SetStreamTime(int time) { time_stream = time; }
	void SetIntCount(int value) { time_intcount = value; }
	int GetIntCount(void) { return time_intcount; }
	int GetPassTick(void) { return pass_tick; }

	//		YM2608�X�e�[�^�X
	void SetChMuteAll(bool sw);
	void SetChMute(int ch, bool sw);
	bool GetChMute(int ch);
	int GetChStatus(int ch);

	//		�f�o�b�O�p
	void Msgf(const char *format, ...);
	char *GetMessageBuffer(void) { return membuf->GetBuffer(); }
	void DumpBin(uint16_t adr, uint16_t length);
	int DeviceCheck(void);
	int GetMessageId() { return msgid; }

	//		ADPCM�p
	int ConvertWAVtoADPCMFile(const char *fname, const char *sname);

	//		BANK�؂�ւ�
	void ClearBank(void);
	void ChangeBank(int bank);

	//		CHDATA�p
	void InitChData(int chmax, int chsize);
	void SetChDataAddress(int ch, int adr);
	uint8_t *GetChData(int ch);
	uint8_t GetChWork(int index);
	void ProcessChData(void);

private:
	//		Z80
	int32_t load(uint16_t adr);
	void store(uint16_t adr, uint8_t data);
	int32_t input(uint16_t adr);
	void output(uint16_t adr, uint8_t data);
	void Halt(void);

	//		������(64K)
	int m_flag;
	int m_option;						// ����I�v�V����
	int m_fastfw;						// ������J�E���g
	int m_fmvol, m_ssgvol;				// �{�����[��
	int bankmode;						// BANK(3=MAIN/012=BGR)
	uint8_t mem[0x10000];				// ���C��������
	uint8_t vram[VMBANK_MAX][0x4000];	// GVRAM(3:�ޔ�/012=BRG)

	//		����
	FM::OPNA *opn;
	SoundDriver::DriverRT *snddrv;

	int sound_reg_select;
	int sound_reg_select2;
	void FMOutData(int data);
	void FMOutData2(int data);
	int FMInData();
	int FMInData2();

	//		OPNA���X�^�b�N
	uint8_t chmute[OPNACH_MAX];
	uint8_t chstat[OPNACH_MAX];
	uint8_t regmap[OPNAREG_MAX];

	//		CH���X�^�b�N
	int channel_max, channel_size;
	uint16_t pchadr[64];
	uint8_t *pchdata;
	uint8_t pchwork[16];

	//		�^�C�}�[
	static void TimeProc(uv_timer_t *tm);

	void UpdateTime(void);

	void StartGenerateSound();
	void EndGenerateSound();
	uint GenerateSoundBlock(float **data);
	float *soundblock;
	enum { soundblocklen = 64 };

	int time_master;
	int time_stream;
	int time_intcount;
	int time_interrupt;
	int pass_tick;
	int64_t last_ft;

	void resetElapsedTime(void);
	int getElapsedTime(void);
	void checkThreadBusy(void);

	uv_loop_t *loop;
	uv_timer_t *timer;

	std::atomic<bool> sending;
	bool busyflag;
	bool playflag;
	bool int3flag;
	int predelay;
	int int3mask;
	int msgid;

	//		���b�Z�[�W�o�b�t�@
	CMemBuf *membuf;

	//		���`�b�v�Ή�
	realchip *m_pChip;
};

#endif
