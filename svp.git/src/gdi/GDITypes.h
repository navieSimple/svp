#ifndef GDITYPES_H
#define GDITYPES_H

#include <QMap>
#include <QString>
#include <stdint.h>

/* Binary Raster Operations (ROP2) */
#define GDI_R2_BLACK			0x01  /* D = 0 */
#define GDI_R2_NOTMERGEPEN		0x02  /* D = ~(D | P) */
#define GDI_R2_MASKNOTPEN		0x03  /* D = D & ~P */
#define GDI_R2_NOTCOPYPEN		0x04  /* D = ~P */
#define GDI_R2_MASKPENNOT		0x05  /* D = P & ~D */
#define GDI_R2_NOT              0x06  /* D = ~D */
#define GDI_R2_XORPEN			0x07  /* D = D ^ P */
#define GDI_R2_NOTMASKPEN		0x08  /* D = ~(D & P) */
#define GDI_R2_MASKPEN			0x09  /* D = D & P */
#define GDI_R2_NOTXORPEN		0x0A  /* D = ~(D ^ P) */
#define GDI_R2_NOP              0x0B  /* D = D */
#define GDI_R2_MERGENOTPEN		0x0C  /* D = D | ~P */
#define GDI_R2_COPYPEN			0x0D  /* D = P */
#define GDI_R2_MERGEPENNOT		0x0E  /* D = P | ~D */
#define GDI_R2_MERGEPEN			0x0F  /* D = P | D */
#define GDI_R2_WHITE			0x10  /* D = 1 */

/* Ternary Raster Operations (ROP3) */
#define GDI_SRCCOPY             0xCC /* D = S */
#define GDI_SRCPAINT			0xEE /* D = S | D	*/
#define GDI_SRCAND              0x88 /* D = S & D	*/
#define GDI_SRCINVERT			0x66 /* D = S ^ D	*/
#define GDI_SRCERASE			0x44 /* D = S & ~D */
#define GDI_NOTSRCCOPY			0x33 /* D = ~S */
#define GDI_NOTSRCERASE			0x11 /* D = ~S & ~D */
#define GDI_MERGECOPY			0xC0 /* D = S & P	*/
#define GDI_MERGEPAINT			0xBB /* D = ~S | D */
#define GDI_PATCOPY             0xF0 /* D = P */
#define GDI_PATPAINT			0xFB /* D = D | (P | ~S) */
#define GDI_PATINVERT			0x5A /* D = P ^ D	*/
#define GDI_DSTINVERT			0x55 /* D = ~D */
#define GDI_BLACKNESS			0x00 /* D = 0 */
#define GDI_WHITENESS			0xFF /* D = 1 */
#define GDI_DSPDxax             0xE2 /* D = (S & P) | (~S & D) */
#define GDI_PSDPxax             0xB8 /* D = (S & D) | (~S & P) */
#define GDI_SPna                0x0C /* D = S & ~P */
#define GDI_DSna                0x22 /* D = D & ~S */
#define GDI_DPa                 0xA0 /* D = D & P */
#define GDI_PDxn                0xA5 /* D = D ^ ~P */

#define GDI_DSxn                0x99 /* D = ~(D ^ S) */
#define GDI_PSDnox              0x2D /* D = P ^ (S | ~D) */
#define GDI_PDSona              0x10 /* D = P & ~(D | S) */
#define GDI_DSPDxox             0x74 /* D = D ^ (S | ( P ^ D)) */
#define GDI_DPSDonox			0x5B /* D = D ^ (P | ~(S | D)) */
#define GDI_SPDSxax             0xAC /* D = S ^ (P & (D ^ S)) */

#define GDI_DPon                0x05 /* D = ~(D | P) */
#define GDI_DPna                0x0A /* D = D & ~P */
#define GDI_Pn                  0x0F /* D = ~P */
#define GDI_PDna                0x50 /* D = P &~D */
#define GDI_DPan                0x5F /* D = ~(D & P) */
#define GDI_DSan                0x77 /* D = ~(D & S) */
#define GDI_DSxn                0x99 /* D = ~(D ^ S) */
#define GDI_DPa                 0xA0 /* D = D & P */
#define GDI_D                   0xAA /* D = D */
#define GDI_DPno                0xAF /* D = D | ~P */
#define GDI_SDno                0xDD /* D = S | ~D */
#define GDI_PDno                0xF5 /* D = P | ~D */
#define GDI_DPo                 0xFA /* D = D | P */

/* Brush Styles */
#define GDI_BS_SOLID			0x00
#define GDI_BS_NULL             0x01
#define GDI_BS_HATCHED			0x02
#define GDI_BS_PATTERN			0x03

/* Hatch Patterns */
#define GDI_HS_HORIZONTAL		0x00
#define GDI_HS_VERTICAL			0x01
#define GDI_HS_FDIAGONAL		0x02
#define GDI_HS_BDIAGONAL		0x03
#define GDI_HS_CROSS			0x04
#define GDI_HS_DIAGCROSS		0x05

/* Pen Styles */
#define GDI_PS_SOLID			0x00
#define GDI_PS_DASH             0x01
#define GDI_PS_NULL             0x05

/* Background Modes */
#define GDI_OPAQUE              0x00000001
#define GDI_TRANSPARENT			0x00000002

/* STROBJ.flAccel constants */
#define SO_FLAG_DEFAULT_PLACEMENT        0x00000001
#define SO_HORIZONTAL                    0x00000002
#define SO_VERTICAL                      0x00000004
#define SO_REVERSED                      0x00000008
#define SO_ZERO_BEARINGS                 0x00000010
#define SO_CHAR_INC_EQUAL_BM_BASE        0x00000020
#define SO_MAXEXT_EQUAL_BM_SIDE          0x00000040
#define SO_DO_NOT_SUBSTITUTE_DEVICE_FONT 0x00000080
#define SO_GLYPHINDEX_TEXTOUT            0x00000100
#define SO_ESC_NOT_ORIENT                0x00000200
#define SO_DXDY                          0x00000400
#define SO_CHARACTER_EXTRA               0x00000800
#define SO_BREAK_EXTRA                   0x00001000

extern QMap<uint8_t, QString> ROP3_TABLE;
extern QMap<uint8_t, QString> ROP2_TABLE;

#endif // GDITYPES_H
