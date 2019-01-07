
//
//	MUCOM88 plugin interface structures
//
#ifndef __CMucom88IF_h
#define __CMucom88IF_h

/*------------------------------------------------------------*/
//	for MUCOM88Win interface
/*------------------------------------------------------------*/

//		�t�@���N�V�����^
//
typedef int (__stdcall *MUCOM88IF_COMMAND) (int,int,int,void *);
typedef int (__stdcall *MUCOM88IF_CALLBACK)(int);

#define MUCOM88IF_VERSION	0x100		// 1.0

#define MUCOM88IF_TYPE_NONE 0
#define MUCOM88IF_TYPE_TOOL 1			// �N��������^�C�v�̊O���c�[��

#define	MUCOM88IF_COMMAND_NONE 0
#define	MUCOM88IF_COMMAND_PASTECLIP 1	// �G�f�B�^�ɃN���b�v�{�[�h�e�L�X�g��\��t��

class mucomvm;
class CMucom;

typedef struct {

	//	Memory Data structure
	//	(*) = DLL���ŏ���������
	//
	int version;						// MUCOM88IF�o�[�W����
	char name[16];						// �v���O�C����(�p����)
	int	type;							// �v���O�C���^�C�v
	char *info;							// �v���O�C�����e�L�X�g(*)
	unsigned char *chmute;				// �`�����l���~���[�g�e�[�u��(*)
	unsigned char *chstat;				// �`�����l���X�e�[�^�X�e�[�u��
	int cur_count;						// ���݉��t���̃J�E���^
	int max_count;						// �J�E���^�̒���

	//	�R�[���o�b�N�t�@���N�V����
	int (__stdcall *if_term) (int);			// ���
	int (__stdcall *if_update) (int);		// �t���[������
	int (__stdcall *if_play) (int);			// ���t�J�n
	int (__stdcall *if_stop) (int);			// ���t��~
	int (__stdcall *if_tool) (int);			// �c�[���N��

	//	�N���X��� (�ύX�̉\��������܂�)
	mucomvm *vm;
	CMucom *mucom;

	//	�ėp�t�@���N�V����(��)
	//
	int(__stdcall* if_editor) (int, int, int, void *);				// �G�f�B�^�n�̃T�[�r�X
	int(__stdcall* if_fmreg) (int, int, int, void *);				// FM�����̃��W�X�^��������

} MUCOM88IF;

typedef int(__stdcall *MUCOM88IF_INIT)(MUCOM88IF *);				// �v���O�C��������

#endif
