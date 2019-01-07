#include "realchip.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

#ifndef _WIN32
#include <dlfcn.h>

static HMODULE LoadLibrary(LPCSTR path)
{
	return dlopen(path, RTLD_LAZY);
}

static BOOL FreeLibrary(HMODULE hmodule)
{
	return dlclose(hmodule) == 0;
}

static FARPROC GetProcAddress(HMODULE hmodule, LPCSTR name)
{
	return (FARPROC)dlsym(hmodule, name);
}

#endif

// �R���X�g���N�^
realchip::realchip()
{
	// �t���O������
	m_IsRealChip = false;
	// ADPCM�p�o�b�t�@�N���A
	memset(m_bADPCMBuff, 0x00, sizeof(m_bADPCMBuff));
}

// �f�X�g���N�^
realchip::~realchip()
{
}

// ������
void realchip::Initialize()
{
	// SCCI��ǂݍ���
	m_hScci = ::LoadLibrary((LPCSTR)"scci.dll");
	if (m_hScci == NULL) {
		return;
	}
	// �T�E���h�C���^�[�t�F�[�X�}�l�[�W���[�擾�p�֐��A�h���X�擾
	SCCIFUNC getSoundInterfaceManager = (SCCIFUNC)(::GetProcAddress(m_hScci, "getSoundInterfaceManager"));
	if (getSoundInterfaceManager == NULL) {
		::FreeLibrary(m_hScci);
		m_hScci = NULL;
		return;
	}
	// �T�E���h�C���^�[�t�F�[�X�}�l�[�W���[�擾
	m_pManager = getSoundInterfaceManager();

	// ������
	m_pManager->initializeInstance();

	// ���Z�b�g����
	m_pManager->reset();

	// �T�E���h�`�b�v�擾
	m_pSoundChip = m_pManager->getSoundChip(SC_TYPE_YM2608, SC_CLOCK_7987200);

	// �T�E���h�`�b�v�̎擾���o���Ȃ��ꍇ
	if (m_pSoundChip == NULL)
	{
		// �T�E���h�}�l�[�W���[��������ďI��
		m_pManager->releaseInstance();
		::FreeLibrary(m_hScci);
		m_hScci = NULL;
		return;
	}
	// �`�b�v���擾�ł����̂Ń��A���`�b�v�œ��삳����
	m_IsRealChip = true;
}

// �J��
void realchip::UnInitialize()
{
	// ���A���`�b�v�����Ȃ瑦�I��
	if (m_IsRealChip == false) return;
	// �T�E���h�`�b�v���J������
	m_pManager->releaseSoundChip(m_pSoundChip);
	m_pSoundChip = NULL;
	// �T�E���h�}�l�[�W���[�J��
	m_pManager->releaseInstance();
	m_pManager = NULL;
	// DLL�J��
	::FreeLibrary(m_hScci);
	m_hScci = NULL;
}

// ���Z�b�g����
void realchip::Reset()
{
	// ���`�b�v�����݂��Ȃ��ꍇ
	if (m_IsRealChip == false) {
		return;
	}
	// ���Z�b�g����
	if (m_pSoundChip) {
		m_pSoundChip->init();
	}
}

// ���A���`�b�v�`�F�b�N
bool realchip::IsRealChip(){
	// ���A���`�b�v�̗L����ԋp����
	return m_IsRealChip;
}

// ���W�X�^�ݒ�
void realchip::SetRegister(DWORD reg, DWORD data) {
	if (m_pSoundChip) {
		m_pSoundChip->setRegister(reg, data);
	}
}

// ADPCM�̓]��
void realchip::SendAdpcmData(void *pData, DWORD size) {
	// ���������邩�`�F�b�N
	if (memcmp(m_bADPCMBuff, pData, size) == 0) {
		// �����������ꍇ�͓]�����Ȃ�
		return;
	}
	// ����������R�s�[����
	memcpy(m_bADPCMBuff, pData, size);
	// ���[���H�̓[�����߂���
	memset(&m_bADPCMBuff[size], 0x00, 0x40000 - size);
	// �]���T�C�Y���v�Z����i�p�f�B���O���l���j
	DWORD transSize = size + ((0x20 - (size & 0x1f)) & 0x1f);

	m_pSoundChip->setRegister(0x100, 0x20);
	m_pSoundChip->setRegister(0x100, 0x21);
	m_pSoundChip->setRegister(0x100, 0x00);
	m_pSoundChip->setRegister(0x110, 0x00);
	m_pSoundChip->setRegister(0x110, 0x80);

	m_pSoundChip->setRegister(0x100, 0x61);
	m_pSoundChip->setRegister(0x100, 0x68);
	m_pSoundChip->setRegister(0x101, 0x00);

	// �A�h���X
	m_pSoundChip->setRegister(0x102, 0x00);
	m_pSoundChip->setRegister(0x103, 0x00);
	m_pSoundChip->setRegister(0x104, 0xff);
	m_pSoundChip->setRegister(0x105, 0xff);
	m_pSoundChip->setRegister(0x10c, 0xff);
	m_pSoundChip->setRegister(0x10d, 0xff);
	// PCM�]��
	for (DWORD dCnt = 0; dCnt < transSize; dCnt++) {
		m_pSoundChip->setRegister(0x108, m_bADPCMBuff[dCnt]);
	}
	// �I��
	m_pSoundChip->setRegister(0x100, 0x00);
	m_pSoundChip->setRegister(SC_WAIT_REG, 16);
	m_pSoundChip->setRegister(0x110, 0x80);
	m_pSoundChip->setRegister(SC_WAIT_REG, 16);

	// �o�b�t�@����ɂȂ�܂ő҂�����
	while (!m_pSoundChip->isBufferEmpty()) {
#ifdef _WIN32
		Sleep(0);
#else
		sched_yield();
#endif
	}
	
}
