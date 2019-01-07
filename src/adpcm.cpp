#include "adpcm.h"
#include <stdlib.h>
#include <string.h>

// �R���X�g���N�^
Adpcm::Adpcm(){
}

// �f�X�g���N�^
Adpcm::~Adpcm(){
}

// ADPCM�ϊ��iDELTA�j
BYTE* Adpcm::waveToAdpcm(void *pData,DWORD dSize,DWORD &dAdpcmSize,DWORD dRate,DWORD dPadSize){
	// RIFF�w�b�_�m�F
	m_pRiffHed = reinterpret_cast<RIFF_HED*>(pData);
	// �w�b�_�`�F�b�N
	if(m_pRiffHed->bID[0] != 'R' && m_pRiffHed->bID[1] != 'I' && m_pRiffHed->bID[2] != 'F' && m_pRiffHed->bID[3] != 'F'){
		return NULL;
	}
	// WAVE�w�b�_�`�F�b�N(CHUNK_HED���p)
	CHUNK_HED *pChunk = reinterpret_cast<CHUNK_HED*>(static_cast<BYTE*>(pData) + 0x00000008);
	if(pChunk->bID[0] != 'W' && pChunk->bID[1] != 'A' && pChunk->bID[2] != 'V' && pChunk->bID[3] != 'E'){
		return NULL;
	}
	// �擪�`�����N��ݒ�
	pChunk = reinterpret_cast<CHUNK_HED*>(reinterpret_cast<DWORD_PTR>(pChunk) + 4);
	m_pWaveChunk = NULL;
	m_pDataChunk = NULL;
	while(reinterpret_cast<DWORD_PTR>(pChunk) < reinterpret_cast<DWORD_PTR>(pData) + dSize)
	{
		// fmt�`�����N�̏ꍇ
		if(pChunk->bID[0] == 'f' && pChunk->bID[1] == 'm' && pChunk->bID[2] == 't' && pChunk->bID[3] == ' '){
			// fmt�`�����N�A�h���X���擾
			m_pWaveChunk = reinterpret_cast<WAVE_CHUNK*>(pChunk);
		}
		// data�`�����N�̏ꍇ
		if(pChunk->bID[0] == 'd' && pChunk->bID[1] == 'a' && pChunk->bID[2] == 't' && pChunk->bID[3] == 'a'){
			// data�`�����N�A�h���X���擾
			m_pDataChunk = reinterpret_cast<DATA_CHUNK*>(pChunk);
		}
		// ���̃`�����N�փA�h���X�����Z����
		pChunk = reinterpret_cast<CHUNK_HED*>(reinterpret_cast<DWORD_PTR>(pChunk) + pChunk->dChunkSize + 8);
	}
	// fmt�`�����N�y��data�`�����N�����݂��邩
	if(m_pWaveChunk == NULL || m_pDataChunk == NULL){
		// ���݂��Ȃ��ꍇ��NG
		return NULL;
	}
	// ���j�APCM�`�F�b�N
	if(m_pWaveChunk->wFmt != 0x0001){
		// ���j�APCM�ȊO��NG�ɂ���
		return NULL;
	}

	// wave�f�[�^���T���v�����O
	DWORD	dPcmSize;
	short	*pPcm = resampling(dPcmSize,dRate,dPadSize);
	// ���T���v�����O�ł��Ȃ������ꍇ
	if(pPcm == NULL){
		return NULL;
	}

	// adpcm�ϊ�
	BYTE	*pAdpcm = new BYTE[dPcmSize / 2];
	encode(pPcm,pAdpcm,dPcmSize);
	dAdpcmSize = dPcmSize / 2;
	delete [] pPcm;
	return	pAdpcm;
}

// ���T���v�����O
short* Adpcm::resampling(DWORD &dSize,DWORD dRate,DWORD dPadSize){
	// �t�H�[�}�b�g�`�F�b�N�i16bit�ȊO��������m�f�j
	if(m_pWaveChunk->wSample != 16){
		return NULL;
	}
	// ���m������
	short *pPcm;
	int		iPcmSize = 0;
	if(m_pWaveChunk->wChannels == 2){
		iPcmSize = static_cast<int>(m_pDataChunk->dSize / 4);
		pPcm = new short[iPcmSize];		// �����̃T�C�Y�ɂȂ�
		short	*pSrc = reinterpret_cast<short*>(&m_pDataChunk->bData[0]);
		short	*pDis = pPcm;
		for(int iCnt = 0; iCnt < iPcmSize; iCnt++){
			int	iPcm = *pSrc++;	// �k
			iPcm += *pSrc++;	// �q
			iPcm /= 2;
			*pDis++ = static_cast<short>(iPcm);
		}
	}else if(m_pWaveChunk->wChannels == 1){
		iPcmSize = static_cast<int>(m_pDataChunk->dSize / 2);
		pPcm = new short[iPcmSize];
		memcpy(pPcm,&m_pDataChunk->bData[0],m_pDataChunk->dSize);
	}else{
		return NULL;
	}
	// ���T���v�����O
	int iSrcRate = static_cast<int>(m_pWaveChunk->dRate);	// wave�̃��[�g
	int iDisRate = (int)dRate;
	int iDiff = 0;
	int	iSampleSize = 0;
	// ���T���v�����O��̃t�@�C���T�C�Y���Z�o
	for(int iCnt = 0; iCnt < iPcmSize; iCnt++){
		iDiff += iDisRate;
		while(iDiff >= iSrcRate){
			// �����o��
			iSampleSize++;
			iDiff -= iSrcRate;
		}
	}
	if(iDiff > 0) iSampleSize++;

	// ���T���v�����O��̃o�b�t�@���쐬����
	int iResampleBuffSize = iSampleSize;
	if(iSampleSize % (dPadSize * 2) > 0) iResampleBuffSize += ((dPadSize * 2) - (iSampleSize % (dPadSize * 2)));
	short *pResampleBuff = new short[iResampleBuffSize];
	memset(pResampleBuff,0,sizeof(short) * iResampleBuffSize);
	// ���T���v�����O���������{����
	short iSampleCnt = 0;
	int iSmple = 0;
	iDiff = 0;
	iSmple = 0;
	short *pResampleDis = pResampleBuff;
	// ���T���v�����O��̃t�@�C���T�C�Y���Z�o
	BOOL	bUpdate = false;
	if(iDisRate != static_cast<int>(m_pWaveChunk->dRate)){
		for(int iCnt = 0; iCnt < iPcmSize; iCnt++){
			iSmple += static_cast<int>(pPcm[iCnt]);		// �T���v�������Z����
			iSampleCnt++;
			iDiff += iDisRate;
			bUpdate = false;
			while(iDiff >= iSrcRate){
				*pResampleDis++ = static_cast<short>(iSmple / iSampleCnt);
				// �����o��
				iDiff -= iSrcRate;
				bUpdate = true;
			}
			if(bUpdate){
				iSampleCnt = 0;
				iSmple = 0;
				bUpdate = false;
			}
		}
		if(iSampleCnt > 0){
			*pResampleDis++ = static_cast<short>(iSmple / iSampleCnt);
		}
	}else{
		for(int iCnt = 0; iCnt < iPcmSize; iCnt++){
			*pResampleDis++ = pPcm[iCnt];
		}
	}
	// �\�[�XPCM�o�b�t�@��j������
	delete [] pPcm;
	dSize = iResampleBuffSize;
	return	pResampleBuff;
}

// �G���R�[�h
int Adpcm::encode(short *pSrc,unsigned char *pDis,DWORD iSampleSize){
	static long stepsizeTable[ 16 ] =
	{
		57, 57, 57, 57, 77,102,128,153,
		57, 57, 57, 57, 77,102,128,153
	};
	int iCnt;
	long i , dn , xn , stepSize;
	unsigned char adpcm;
	unsigned char adpcmPack = 0;

	// �����l�ݒ�
	xn			= 0;
	stepSize	= 127;
	
	for( iCnt = 0 ; iCnt < static_cast<int>(iSampleSize) ; iCnt++ ){
		// �G���R�[�h����
		dn = *pSrc - xn;		// �������o
		pSrc++;
		i = (abs(dn) << 16) / (stepSize << 14);
		if(i > 7){
			i = 7;
		}
		adpcm = (unsigned char)i;
		i = (adpcm * 2 + 1) * stepSize >> 3;
		if(dn < 0){
			adpcm |= 0x8;
			xn -= i;
		}else{
			xn += i;
		}
		stepSize = (stepsizeTable[adpcm] * stepSize) / 64;
		if(stepSize < 127){
			stepSize = 127;
		}else if( stepSize > 24576 ){
			stepSize = 24576;
		}
		// ADPCM�f�[�^����
		if((iCnt & 0x01) == 0){
			adpcmPack = (adpcm << 4);
		}else{
			adpcmPack |= adpcm;
			*pDis = adpcmPack;
			pDis++;
		}
	}
	return 0;
}

