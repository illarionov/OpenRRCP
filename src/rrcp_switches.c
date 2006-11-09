/*
    This file is part of openrrcp

    pcapsipdump is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    pcapsipdump is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    ---

    You can send your updates, patches and suggestions on this software
    to it's original author, Andrew Chernyak (nording@yandex.ru)
    This would be appreciated, however not required.
*/

#include "rrcp_switches.h"

unsigned int switchtype;

struct switchtype_t switchtypes[5] = {
    {
	"generic",
	"rtl8316b",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,0,0}
    },
    {
	"generic",
	"rtl8326",
	"unknown",
	"rtl8326",
	rtl8326,
	26,
	{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,0}
    },
    {
	"dlink",
	"des1016d",
	"D1",
	"rtl8316b",
	rtl8316b,
	16,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,0,0,0,0,0,0,0,0,0,0,0}
    },
    {
	"dlink",
	"des1024d",
	"unknown",
	"rtl8326",
	rtl8326,
	24,
	{2,1,4,3,6,5,8,7,10,9,12,11,14,13,16,15,18,17,20,19,22,21,24,23,0,0,0}
    },
    {
	"compex",
	"ps2216",
	"unknown",
	"rtl8316b",
	rtl8316b,
	16,
	{16,13,11,9,7,5,3,1,15,14,12,10,8,6,4,2,0,0,0,0,0,0,0,0,0,0,0}
    }
};

const int switchtype_n = 5;
