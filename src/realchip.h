#pragma once
#include "types.h"
#include "scci.h"
#include "SCCIDefines.h"

// ���A���`�b�v����p
class realchip
{
public:
	// �R���X�g���N�^
	realchip();
	// �f�X�g���N�^
	~realchip();
	// ������
	void Initialize();
	// �J��
	void UnInitialize();
	// ���Z�b�g
	void Reset();
	// ���A���`�b�v�̎g�p�`�F�b�N
	bool IsRealChip();
	// ���W�X�^�̐ݒ�
	void SetRegister(DWORD reg, DWORD data);
	// ADPCM�̓]��
	void SendAdpcmData(void *pData, DWORD size);

private:
	// SCCI�֘A
	HMODULE					m_hScci;
	SoundInterfaceManager	*m_pManager;
	SoundChip				*m_pSoundChip;
	// ���`�b�v�t���O
	bool					m_IsRealChip;
	// ADPCM�p
	BYTE					m_bADPCMBuff[0x40000];
};
