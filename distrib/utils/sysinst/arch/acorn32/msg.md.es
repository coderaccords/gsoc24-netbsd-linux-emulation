/*	$NetBSD: msg.md.es,v 1.2 2005/08/26 16:32:31 xtraeme Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Based on code written by Philip A. Nelson for Piermont Information
 * Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* arm32 machine dependent messages, spanish */


message md_hello
{Si ha iniciado desde disquette, ahora deber�a retirar el disco.

}

message fullpart
{Ahora vamos a instalar NetBSD en el disco %s. Deber�a escoger
entre instalar NetBSD en el disco entero o en una parte del disco.
�Que le gustar�a hacer?
}

message badreadbb
{No se puede leer el bloque de arranque filecore
}

message badreadriscix
{No se puede leer la tabla de particiones RISCiX
}

message notnetbsdriscix
{No se ha encontrado ninguna partici�n NetBSD en la tabla de
particiones RISCiX - No se puede etiquetar
}

message notnetbsd
{No se ha encontrado ninguna partici�n NetBSD (�disco solo de filecore?)
 - No se puede etiquetar
}

message dobootblks
{Instalando bloques de arranque en %s...
}

message set_kernel_1
{Nucleo (GENERIC)}
message set_kernel_2
{Nucleo (GENERIC_RPC_WSCONS)}
message set_kernel_3 
{Nucleo (GENERIC_NC)}
message set_kernel_4
{Nucleo (GENERIC_NC_WSCONS)}


message arm32fspart
{Ahora tenemos nuestras particiones NetBSD en %s como sigue (Tama�o y
Compensaci�n en %s):
}
