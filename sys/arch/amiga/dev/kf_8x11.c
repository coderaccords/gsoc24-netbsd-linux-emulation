/*
 *	From ftp.funet.fi:pub/amiga/system/fonts/Systemfonts1.lha
 *	Font: system.font/11
 *	$Id: kf_8x11.c,v 1.2 1994/05/08 05:53:25 chopps Exp $
 */
#ifdef KFONT_8X11
short kernel_font_boldsmear_8x11 = 1;
unsigned char kernel_font_width_8x11 = 8;
unsigned char kernel_font_height_8x11 = 11;
unsigned char kernel_font_baseline_8x11 = 8;
unsigned char kernel_font_lo_8x11 = 32;
unsigned char kernel_font_hi_8x11 = 255;

unsigned char kernel_cursor_8x11[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

unsigned char kernel_font_8x11[] = {
/*   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* ! */ 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x00,
/* " */ 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* # */ 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x00,
/* $ */ 0x18, 0x3c, 0x66, 0x60, 0x3c, 0x06, 0x66, 0x3c, 0x18, 0x00, 0x00,
/* % */ 0x00, 0x40, 0xa6, 0x4c, 0x18, 0x30, 0x64, 0xca, 0x04, 0x00, 0x00,
/* & */ 0x78, 0xcc, 0xcc, 0xc8, 0x76, 0xdc, 0xcc, 0xcc, 0x76, 0x00, 0x00,
/* ' */ 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* ( */ 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00, 0x00,
/* ) */ 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00, 0x00,
/* * */ 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00,
/* + */ 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
/* , */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00,
/* - */ 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* . */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
/* / */ 0x00, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00, 0x00, 0x00,
/* 0 */ 0x7c, 0xc6, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0x7c, 0x00, 0x00,
/* 1 */ 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* 2 */ 0x7c, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xfe, 0x00, 0x00,
/* 3 */ 0x7c, 0xc6, 0x06, 0x06, 0x1c, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00,
/* 4 */ 0x0c, 0x0c, 0xcc, 0xcc, 0xcc, 0xfe, 0x0c, 0x0c, 0x0c, 0x00, 0x00,
/* 5 */ 0xfe, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00,
/* 6 */ 0x3c, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* 7 */ 0xfe, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* 8 */ 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* 9 */ 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x0c, 0x78, 0x00, 0x00,
/* : */ 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
/* ; */ 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00,
/* < */ 0x00, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00,
/* = */ 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* > */ 0x00, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00, 0x00, 0x00,
/* ? */ 0x7c, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00,
/* @ */ 0x7c, 0xc6, 0xc6, 0xde, 0xd6, 0xde, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* A */ 0x7c, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* B */ 0xfc, 0xc6, 0xc6, 0xc6, 0xfc, 0xc6, 0xc6, 0xc6, 0xfc, 0x00, 0x00,
/* C */ 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00,
/* D */ 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0x00, 0x00,
/* E */ 0xfe, 0xc0, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* F */ 0xfe, 0xc0, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
/* G */ 0x7c, 0xc6, 0xc0, 0xc0, 0xce, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* H */ 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* I */ 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00,
/* J */ 0x06, 0x06, 0x06, 0x06, 0x06, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* K */ 0xc6, 0xc6, 0xc6, 0xcc, 0xf8, 0xcc, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* L */ 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* M */ 0x82, 0x82, 0xc6, 0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0x00, 0x00,
/* N */ 0x86, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc2, 0x00, 0x00,
/* O */ 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* P */ 0xfc, 0xc6, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
/* Q */ 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xde, 0x7c, 0x06, 0x03,
/* R */ 0xfc, 0xc6, 0xc6, 0xc6, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* S */ 0x7c, 0xc6, 0xc0, 0xc0, 0x7c, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00,
/* T */ 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* U */ 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* V */ 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x10, 0x00, 0x00,
/* W */ 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0xc6, 0x00, 0x00,
/* X */ 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* Y */ 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* Z */ 0xfe, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* [ */ 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00,
/* \ */ 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00, 0x00, 0x00,
/* ] */ 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00,
/* ^ */ 0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* _ */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
/* ` */ 0x18, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* a */ 0x00, 0x00, 0x00, 0x7c, 0x06, 0x7e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* b */ 0xc0, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0x00, 0x00,
/* c */ 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00,
/* d */ 0x06, 0x06, 0x06, 0x7e, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* e */ 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* f */ 0x1c, 0x36, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00,
/* g */ 0x00, 0x00, 0x00, 0x7e, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
/* h */ 0xc0, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* i */ 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* j */ 0x06, 0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0xc6, 0x7c,
/* k */ 0xc0, 0xc0, 0xc0, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0xc6, 0x00, 0x00,
/* l */ 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* m */ 0x00, 0x00, 0x00, 0xec, 0xfe, 0xd6, 0xd6, 0xc6, 0xc6, 0x00, 0x00,
/* n */ 0x00, 0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* o */ 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* p */ 0x00, 0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0,
/* q */ 0x00, 0x00, 0x00, 0x7e, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06,
/* r */ 0x00, 0x00, 0x00, 0xfc, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
/* s */ 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x70, 0x1c, 0xc6, 0x7c, 0x00, 0x00,
/* t */ 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x0c, 0x00, 0x00,
/* u */ 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* v */ 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x10, 0x00, 0x00,
/* w */ 0x00, 0x00, 0x00, 0xc6, 0xd6, 0xd6, 0xd6, 0x6c, 0x6c, 0x00, 0x00,
/* x */ 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00, 0x00,
/* y */ 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
/* z */ 0x00, 0x00, 0x00, 0xfe, 0x0c, 0x18, 0x30, 0x60, 0xfe, 0x00, 0x00,
/* { */ 0x0e, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00,
/* | */ 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* } */ 0x70, 0x18, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00,
/* ~ */ 0x72, 0x9c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  */ 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x0c, 0x0c, 0x3e, 0x6c, 0x6c, 0x6c, 0x3e, 0x0c, 0x0c, 0x00, 0x00,
/* � */ 0x1c, 0x36, 0x30, 0x30, 0x78, 0x30, 0x30, 0x30, 0x7e, 0x00, 0x00,
/* � */ 0x66, 0x66, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x00, 0x00,
/* � */ 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x3c, 0x60, 0x60, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x06, 0x06, 0x3c,
/* � */ 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x3c, 0x42, 0x99, 0xa1, 0xa1, 0xa1, 0x99, 0x42, 0x3c, 0x00,
/* � */ 0x3e, 0x66, 0x66, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x7e, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa9, 0xa5, 0x42, 0x3c, 0x00,
/* � */ 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x06, 0x0c, 0x18, 0x30, 0x7e, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc0, 0xc0,
/* � */ 0x7e, 0xca, 0xca, 0xca, 0x7a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x00,
/* � */ 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x30,
/* � */ 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x66, 0x3c, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x40, 0xc6, 0x4c, 0x58, 0x32, 0x6a, 0xce, 0x02, 0x02, 0x00, 0x00,
/* � */ 0x40, 0xc6, 0x4c, 0x58, 0x3e, 0x72, 0xc4, 0x08, 0x1e, 0x00, 0x00,
/* � */ 0xc0, 0x63, 0xc6, 0x6c, 0xd9, 0x35, 0x67, 0xc1, 0x01, 0x00, 0x00,
/* � */ 0x18, 0x18, 0x00, 0x18, 0x30, 0x60, 0xc0, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0x00, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x38, 0x7c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x7f, 0xcc, 0xcc, 0xcc, 0xce, 0xfc, 0xcc, 0xcc, 0xcf, 0x00, 0x00,
/* � */ 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x30,
/* � */ 0x30, 0x18, 0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* � */ 0x18, 0x30, 0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* � */ 0xc6, 0x00, 0xfe, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xfe, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00,
/* � */ 0x66, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00,
/* � */ 0xf8, 0xcc, 0xc6, 0xc6, 0xf6, 0xc6, 0xc6, 0xcc, 0xf8, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc2, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x18, 0x30, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00,
/* � */ 0x7c, 0xc6, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0xc3, 0xc3, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0xc0, 0xc0, 0xfe, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0, 0xc0, 0xc0, 0xc0,
/* � */ 0xfc, 0xc6, 0xc6, 0xc6, 0xdc, 0xc6, 0xc6, 0xc6, 0xdc, 0xc0, 0xc0,
/* � */ 0x30, 0x18, 0x00, 0x7c, 0x06, 0x7e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0x7c, 0x06, 0x7e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0x7c, 0x06, 0x3e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0x00, 0x7c, 0x06, 0x3e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0x7c, 0x06, 0x3e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x3c, 0x7c, 0x06, 0x7e, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x7e, 0x1b, 0x7f, 0xd8, 0xd8, 0x77, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x18, 0x30,
/* � */ 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0x7c, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x3c, 0x66, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x66, 0x66, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
/* � */ 0x6c, 0x30, 0xd8, 0x0c, 0x7e, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0x00, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x72, 0x9c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
/* � */ 0x00, 0x00, 0x00, 0x7c, 0xce, 0xde, 0xf6, 0xe6, 0x7c, 0x00, 0x00,
/* � */ 0x30, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x38, 0x6c, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00,
/* � */ 0x0c, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
/* � */ 0xc0, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0xc0,
/* � */ 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x7c,
};
#endif /* KFONT_8x11 */
