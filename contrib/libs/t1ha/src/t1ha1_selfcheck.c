/* 
 *  Copyright (c) 2016-2020 Positive Technologies, https://www.ptsecurity.com,
 *  Fast Positive Hash. 
 * 
 *  Portions Copyright (c) 2010-2020 Leonid Yuriev <leo@yuriev.ru>,
 *  The 1Hippeus project (t1h). 
 * 
 *  This software is provided 'as-is', without any express or implied 
 *  warranty. In no event will the authors be held liable for any damages 
 *  arising from the use of this software. 
 * 
 *  Permission is granted to anyone to use this software for any purpose, 
 *  including commercial applications, and to alter it and redistribute it 
 *  freely, subject to the following restrictions: 
 * 
 *  1. The origin of this software must not be misrepresented; you must not 
 *     claim that you wrote the original software. If you use this software 
 *     in a product, an acknowledgement in the product documentation would be 
 *     appreciated but is not required. 
 *  2. Altered source versions must be plainly marked as such, and must not be 
 *     misrepresented as being the original software. 
 *  3. This notice may not be removed or altered from any source distribution. 
 */ 
 
/* 
 * t1ha = { Fast Positive Hash, aka "Позитивный Хэш" } 
 * by [Positive Technologies](https://www.ptsecurity.ru) 
 * 
 * Briefly, it is a 64-bit Hash Function: 
 *  1. Created for 64-bit little-endian platforms, in predominantly for x86_64, 
 *     but portable and without penalties it can run on any 64-bit CPU. 
 *  2. In most cases up to 15% faster than City64, xxHash, mum-hash, metro-hash 
 *     and all others portable hash-functions (which do not use specific 
 *     hardware tricks). 
 *  3. Not suitable for cryptography. 
 * 
 * The Future will (be) Positive. Всё будет хорошо.
 * 
 * ACKNOWLEDGEMENT: 
 * The t1ha was originally developed by Leonid Yuriev (Леонид Юрьев) 
 * for The 1Hippeus project - zerocopy messaging in the spirit of Sparta! 
 */ 
 
#ifndef T1HA1_DISABLED 
#include "t1ha_bits.h" 
#include "t1ha_selfcheck.h" 
 
/* *INDENT-OFF* */ 
/* clang-format off */ 
 
const uint64_t t1ha_refval_64le[81] = { 0, 
  0x6A580668D6048674, 0xA2FE904AFF0D0879, 0xE3AB9C06FAF4D023, 0x6AF1C60874C95442, 
  0xB3557E561A6C5D82, 0x0AE73C696F3D37C0, 0x5EF25F7062324941, 0x9B784F3B4CE6AF33, 
  0x6993BB206A74F070, 0xF1E95DF109076C4C, 0x4E1EB70C58E48540, 0x5FDD7649D8EC44E4, 
  0x559122C706343421, 0x380133D58665E93D, 0x9CE74296C8C55AE4, 0x3556F9A5757AB6D0, 
  0xF62751F7F25C469E, 0x851EEC67F6516D94, 0xED463EE3848A8695, 0xDC8791FEFF8ED3AC, 
  0x2569C744E1A282CF, 0xF90EB7C1D70A80B9, 0x68DFA6A1B8050A4C, 0x94CCA5E8210D2134, 
  0xF5CC0BEABC259F52, 0x40DBC1F51618FDA7, 0x0807945BF0FB52C6, 0xE5EF7E09DE70848D, 
  0x63E1DF35FEBE994A, 0x2025E73769720D5A, 0xAD6120B2B8A152E1, 0x2A71D9F13959F2B7, 
  0x8A20849A27C32548, 0x0BCBC9FE3B57884E, 0x0E028D255667AEAD, 0xBE66DAD3043AB694, 
  0xB00E4C1238F9E2D4, 0x5C54BDE5AE280E82, 0x0E22B86754BC3BC4, 0x016707EBF858B84D, 
  0x990015FBC9E095EE, 0x8B9AF0A3E71F042F, 0x6AA56E88BD380564, 0xAACE57113E681A0F, 
  0x19F81514AFA9A22D, 0x80DABA3D62BEAC79, 0x715210412CABBF46, 0xD8FA0B9E9D6AA93F, 
  0x6C2FC5A4109FD3A2, 0x5B3E60EEB51DDCD8, 0x0A7C717017756FE7, 0xA73773805CA31934, 
  0x4DBD6BB7A31E85FD, 0x24F619D3D5BC2DB4, 0x3E4AF35A1678D636, 0x84A1A8DF8D609239, 
  0x359C862CD3BE4FCD, 0xCF3A39F5C27DC125, 0xC0FF62F8FD5F4C77, 0x5E9F2493DDAA166C, 
  0x17424152BE1CA266, 0xA78AFA5AB4BBE0CD, 0x7BFB2E2CEF118346, 0x647C3E0FF3E3D241, 
  0x0352E4055C13242E, 0x6F42FC70EB660E38, 0x0BEBAD4FABF523BA, 0x9269F4214414D61D, 
  0x1CA8760277E6006C, 0x7BAD25A859D87B5D, 0xAD645ADCF7414F1D, 0xB07F517E88D7AFB3, 
  0xB321C06FB5FFAB5C, 0xD50F162A1EFDD844, 0x1DFD3D1924FBE319, 0xDFAEAB2F09EF7E78, 
  0xA7603B5AF07A0B1E, 0x41CD044C0E5A4EE3, 0xF64D2F86E813BF33, 0xFF9FDB99305EB06A 
}; 
 
const uint64_t t1ha_refval_64be[81] = { 0, 
  0x6A580668D6048674, 0xDECC975A0E3B8177, 0xE3AB9C06FAF4D023, 0xE401FA8F1B6AF969, 
  0x67DB1DAE56FB94E3, 0x1106266A09B7A073, 0x550339B1EF2C7BBB, 0x290A2BAF590045BB, 
  0xA182C1258C09F54A, 0x137D53C34BE7143A, 0xF6D2B69C6F42BEDC, 0x39643EAF2CA2E4B4, 
  0x22A81F139A2C9559, 0x5B3D6AEF0AF33807, 0x56E3F80A68643C08, 0x9E423BE502378780, 
  0xCDB0986F9A5B2FD5, 0xD5B3C84E7933293F, 0xE5FB8C90399E9742, 0x5D393C1F77B2CF3D, 
  0xC8C82F5B2FF09266, 0xACA0230CA6F7B593, 0xCB5805E2960D1655, 0x7E2AD5B704D77C95, 
  0xC5E903CDB8B9EB5D, 0x4CC7D0D21CC03511, 0x8385DF382CFB3E93, 0xF17699D0564D348A, 
  0xF77EE7F8274A4C8D, 0xB9D8CEE48903BABE, 0xFE0EBD2A82B9CFE9, 0xB49FB6397270F565, 
  0x173735C8C342108E, 0xA37C7FBBEEC0A2EA, 0xC13F66F462BB0B6E, 0x0C04F3C2B551467E, 
  0x76A9CB156810C96E, 0x2038850919B0B151, 0xCEA19F2B6EED647B, 0x6746656D2FA109A4, 
  0xF05137F221007F37, 0x892FA9E13A3B4948, 0x4D57B70D37548A32, 0x1A7CFB3D566580E6, 
  0x7CB30272A45E3FAC, 0x137CCFFD9D51423F, 0xB87D96F3B82DF266, 0x33349AEE7472ED37, 
  0x5CC0D3C99555BC07, 0x4A8F4FA196D964EF, 0xE82A0D64F281FBFA, 0x38A1BAC2C36823E1, 
  0x77D197C239FD737E, 0xFB07746B4E07DF26, 0xC8A2198E967672BD, 0x5F1A146D143FA05A, 
  0x26B877A1201AB7AC, 0x74E5B145214723F8, 0xE9CE10E3C70254BC, 0x299393A0C05B79E8, 
  0xFD2D2B9822A5E7E2, 0x85424FEA50C8E50A, 0xE6839E714B1FFFE5, 0x27971CCB46F9112A, 
  0xC98695A2E0715AA9, 0x338E1CBB4F858226, 0xFC6B5C5CF7A8D806, 0x8973CAADDE8DA50C, 
  0x9C6D47AE32EBAE72, 0x1EBF1F9F21D26D78, 0x80A9704B8E153859, 0x6AFD20A939F141FB, 
  0xC35F6C2B3B553EEF, 0x59529E8B0DC94C1A, 0x1569DF036EBC4FA1, 0xDA32B88593C118F9, 
  0xF01E4155FF5A5660, 0x765A2522DCE2B185, 0xCEE95554128073EF, 0x60F072A5CA51DE2F 
}; 
 
/* *INDENT-ON* */ 
/* clang-format on */ 
 
__cold int t1ha_selfcheck__t1ha1_le(void) { 
  return t1ha_selfcheck(t1ha1_le, t1ha_refval_64le); 
} 
 
__cold int t1ha_selfcheck__t1ha1_be(void) { 
  return t1ha_selfcheck(t1ha1_be, t1ha_refval_64be); 
} 
 
__cold int t1ha_selfcheck__t1ha1(void) { 
  return t1ha_selfcheck__t1ha1_le() | t1ha_selfcheck__t1ha1_be(); 
} 
 
#endif /* T1HA1_DISABLED */ 
