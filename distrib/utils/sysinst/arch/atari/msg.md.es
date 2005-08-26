/*	$NetBSD: msg.md.es,v 1.2 2005/08/26 16:32:32 xtraeme Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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

/* MD Message catalog -- spanish, atari version */

message md_hello
{Si ha iniciado desde disquette, ahora deber�a retirar el disco.

}

message dobootblks
{Instalando bloques de arranque en %s...
}

message infoahdilabel
{Ahora tiene que preparar su disco raiz para la instalaci�n de NetBSD. Esto
se refiere mas a fondo como 'etiquetar' un disco.

Primero tiene la posibilidad de editar o crear un particionaje compatible
con AHDI en el disco de instalaci�n. Note que NetBSD puede hacerlo sin
particiones AHDI, compruebe la documentaci�n.
Si quiere usar un particionaje compatible con AHDI, tiene que asignar algunas
particiones a NetBSD antes de que NetBSD pueda usar el disco. Cambie la 'id'
de todas las particiones en las que quiera usar sistemas de archivos NetBSD
a 'NBD'. Cambie la 'id' de la partici�n en la que quiera usar swap a 'SWP'.

Quiere un particionaje compatible con AHDI? }

message set_kernel_1
{N�cleo (TT030/Falcon)}
message set_kernel_2
{N�cleo (Hades)}
message set_kernel_3 
{N�cleo (Milan ISA)}
message set_kernel_4
{N�cleo (Milan PCI)}
