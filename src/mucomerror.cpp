
//
//	MUCOM88 debug support
//	(エラー処理)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mucomerror.h"

/*------------------------------------------------------------*/
/*
		system data
*/
/*------------------------------------------------------------*/


/*------------------------------------------------------------*/
/*
		interface
*/
/*------------------------------------------------------------*/

static const char *orgmsg[]={
	"",
	u8"ﾌﾞﾝﾎﾟｳ ﾆ ｱﾔﾏﾘ ｶﾞ ｱﾘﾏｽ",
	u8"ﾊﾟﾗﾒｰﾀﾉ ｱﾀｲ ｶﾞ ｲｼﾞｮｳﾃﾞｽ",
	u8" ]  ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ",
	u8" [  ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ",
	u8"ｵﾝｼｮｸ ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ",
	u8"ｵｸﾀｰﾌﾞ ｶﾞ ｷﾃｲﾊﾝｲ ｦ ｺｴﾃﾏｽ",
	u8"ﾘｽﾞﾑ ｶﾞ ｸﾛｯｸ ﾉ ﾁｦ ｺｴﾃﾏｽ",
	u8"[ ] ﾅｲ ﾆ / ﾊ ﾋﾄﾂﾀﾞｹﾃﾞｽ",
	u8"ﾊﾟﾗﾒｰﾀ ｶﾞ ﾀﾘﾏｾﾝ",
	u8"ｺﾉﾁｬﾝﾈﾙ ﾃﾞﾊ ﾂｶｴﾅｲ ｺﾏﾝﾄﾞｶﾞｱﾘﾏｽ",
	u8"[ ] ﾉ ﾈｽﾄﾊ 16ｶｲ ﾏﾃﾞﾃﾞｽ",
	u8"ｵﾝｼｮｸ ﾃﾞｰﾀ ｶﾞ ﾗｲﾌﾞﾗﾘ ﾆ ｿﾝｻﾞｲｼﾏｾﾝ",
	u8"ｶｳﾝﾀｰ ｵｰﾊﾞｰﾌﾛｰ",
	u8"ﾓｰﾄﾞ ｴﾗｰ",
	u8"ｵﾌﾞｼﾞｪｸﾄ ﾘｮｳｲｷ ｦ ｺｴﾏｼﾀ",
	u8"ﾃｲｷﾞｼﾃﾅｲ ﾏｸﾛﾅﾝﾊﾞｰｶﾞｱﾘﾏｽ",
	u8"ﾏｸﾛｴﾝﾄﾞｺｰﾄﾞ ｶﾞ ｱﾘﾏｾﾝ",
};

static const char *err[]={
	"",												// 0
	"Syntax error",									// 1 'ﾌﾞﾝﾎﾟｳ ﾆ ｱﾔﾏﾘ ｶﾞ ｱﾘﾏｽ',0
	"Illegal parameter",							// 2 'ﾊﾟﾗﾒｰﾀﾉ ｱﾀｲ ｶﾞ ｲｼﾞｮｳﾃﾞｽ',0
	"Too many loop end ']'",						// 3 ' ]  ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ',0
	"Too many loops '['",							// 4 ' [  ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ',0
	"Too many FM voice '@'",						// 5 'ｵﾝｼｮｸ ﾉ ｶｽﾞ ｶﾞ ｵｵｽｷﾞﾏｽ',0
	"Octave out of range",							// 6 'ｵｸﾀｰﾌﾞ ｶﾞ ｷﾃｲﾊﾝｲ ｦ ｺｴﾃﾏｽ',0
	"Rhythm too fast, Clock over",					// 7 'ﾘｽﾞﾑ ｶﾞ ｸﾛｯｸ ﾉ ﾁｦ ｺｴﾃﾏｽ',0
	"Unexpected loop escape '/'",					// 8 '[ ] ﾅｲ ﾆ / ﾊ ﾋﾄﾂﾀﾞｹﾃﾞｽ',0
	"Need more parameter",							// 9 'ﾊﾟﾗﾒｰﾀ ｶﾞ ﾀﾘﾏｾﾝ',0
	"Command not supported at this CH",				// 10 'ｺﾉﾁｬﾝﾈﾙ ﾃﾞﾊ ﾂｶｴﾅｲ ｺﾏﾝﾄﾞｶﾞｱﾘﾏｽ',0
	"Too many loop nest '['-']'",					// 11 '[ ] ﾉ ﾈｽﾄﾊ 16ｶｲ ﾏﾃﾞﾃﾞｽ',0
	"FM Voice not found",							// 12 'ｵﾝｼｮｸ ﾃﾞｰﾀ ｶﾞ ﾗｲﾌﾞﾗﾘ ﾆ ｿﾝｻﾞｲｼﾏｾﾝ',0
	"Counter overflow",								// 13 'ｶｳﾝﾀｰ ｵｰﾊﾞｰﾌﾛｰ',0
	"Mode error",									// 14 'ﾓｰﾄﾞ ｴﾗｰ',0
	"Music data overflow",							// 15 'ｵﾌﾞｼﾞｪｸﾄ ﾘｮｳｲｷ ｦ ｺｴﾏｼﾀ',0
	"Undefined macro number",						// 16 'ﾃｲｷﾞｼﾃﾅｲ ﾏｸﾛﾅﾝﾊﾞｰｶﾞｱﾘﾏｽ',0
	"Macro end code not found",						// 17 'ﾏｸﾛｴﾝﾄﾞｺｰﾄﾞ ｶﾞ ｱﾘﾏｾﾝ',0
	"*"
};

static const char *err_jpn[] = {
	"",
	u8"文法に誤りがあります",
	u8"パラメーターの値が異常です",
	u8" ]の数が多すぎます",
	u8" [の数が多すぎます",
	u8"音色の数が多すぎます",
	u8"オクターブが規定範囲を超えてます",
	u8"リズムがクロックの値を超えてます",
	u8"[ ] 内に / は1つだけです",
	u8"パラメーターがたりません",
	u8"このチャンネルでは使えないコマンドがあります",
	u8"[ ] のネストは16回までです",
	u8"音色のデータがライブラリに存在しません",
	u8"カウンターオーバーフロー",
	u8"モードエラー",
	u8"オブジェクト領域を超えました",
	u8"定義していないマクロナンバーがあります",
	u8"マクロエンドコードがありません",
};

const char *mucom_geterror(int error)
{
	if ((error<0)||(error>=MUCOMERR_MAX)) return err[0];
	return err[error];
}

const char *mucom_geterror_j(int error)
{
	if ((error<0) || (error >= MUCOMERR_MAX)) return err[0];
	return err_jpn[error];
}

int mucom_geterror(const char *orgerror)
{
	int i;
	for (i = 1; i < MUCOMERR_MAX; i++) {
		const char *p = orgmsg[i];
		if (strcmp(p, orgerror) == 0) return i;
	}
	return 0;
}

